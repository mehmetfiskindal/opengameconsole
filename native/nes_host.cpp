#include "nes_host.h"

#include "libretro.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

extern "C" const unsigned char opengameconsole_pad_demo_rom[];
extern "C" const size_t opengameconsole_pad_demo_rom_size;
extern "C" int sailfish_canvas_width();
extern "C" int sailfish_canvas_height();
extern "C" void sailfish_display_write_rgb565(const std::uint16_t *pixels, int x, int y, int width, int height);

namespace {

constexpr unsigned kMaxFrameWidth = 256;
constexpr unsigned kMaxFrameHeight = 240;
constexpr int kScreenBandPercent = 62;
constexpr int kMaxFramesPerLoop = 2;
constexpr std::uint32_t kAudioQueueLimitMs = 120;

struct ButtonName {
	const char *name;
	unsigned id;
};

constexpr ButtonName kButtons[] = {
	{"up", RETRO_DEVICE_ID_JOYPAD_UP},
	{"down", RETRO_DEVICE_ID_JOYPAD_DOWN},
	{"left", RETRO_DEVICE_ID_JOYPAD_LEFT},
	{"right", RETRO_DEVICE_ID_JOYPAD_RIGHT},
	{"a", RETRO_DEVICE_ID_JOYPAD_A},
	{"b", RETRO_DEVICE_ID_JOYPAD_B},
	{"start", RETRO_DEVICE_ID_JOYPAD_START},
	{"select", RETRO_DEVICE_ID_JOYPAD_SELECT},
};

bool g_running = false;
std::uint32_t g_buttons = 0;
std::uint16_t g_frame[kMaxFrameWidth * kMaxFrameHeight];
unsigned g_frameWidth = 0;
unsigned g_frameHeight = 0;
bool g_frameChanged = false;
double g_fps = 60.0;
std::int64_t g_startNs = 0;
std::int64_t g_framesRun = 0;
std::vector<std::uint16_t> g_scaled;
SDL_AudioDeviceID g_audio = 0;
std::uint32_t g_audioBytesPerSecond = 0;
retro_game_info_ext g_gameInfoExt{};

std::int64_t nowNs()
{
	timespec ts{};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return static_cast<std::int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

bool romLooksValid(const unsigned char *rom, std::size_t size)
{
	return size >= 16 + 16384 && std::memcmp(rom, "NES\x1A", 4) == 0;
}

bool environment(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return *static_cast<const retro_pixel_format *>(data) == RETRO_PIXEL_FORMAT_RGB565;
	case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
		*static_cast<const retro_game_info_ext **>(data) = &g_gameInfoExt;
		return true;
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		*static_cast<bool *>(data) = true;
		return true;
	default:
		return false;
	}
}

void videoRefresh(const void *data, unsigned width, unsigned height, std::size_t pitch)
{
	if (!data) return;
	width = std::min(width, kMaxFrameWidth);
	height = std::min(height, kMaxFrameHeight);
	const auto *src = static_cast<const std::uint8_t *>(data);
	for (unsigned y = 0; y < height; y++)
		std::memcpy(g_frame + y * kMaxFrameWidth, src + y * pitch, width * sizeof(std::uint16_t));
	g_frameWidth = width;
	g_frameHeight = height;
	g_frameChanged = true;
}

void queueAudio(const std::int16_t *samples, std::size_t frames)
{
	if (!g_audio || frames == 0) return;
	if (SDL_GetQueuedAudioSize(g_audio) > g_audioBytesPerSecond * kAudioQueueLimitMs / 1000) return;
	SDL_QueueAudio(g_audio, samples, static_cast<Uint32>(frames * 2 * sizeof(std::int16_t)));
}

void audioSample(std::int16_t left, std::int16_t right)
{
	const std::int16_t frame[2] = {left, right};
	queueAudio(frame, 1);
}

std::size_t audioSampleBatch(const std::int16_t *data, std::size_t frames)
{
	queueAudio(data, frames);
	return frames;
}

void inputPoll() {}

std::int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (port != 0 || device != RETRO_DEVICE_JOYPAD || index != 0) return 0;
	if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return static_cast<std::int16_t>(g_buttons);
	if (id >= 16) return 0;
	return static_cast<std::int16_t>((g_buttons >> id) & 1u);
}

void openAudio(double sampleRate)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		std::fprintf(stderr, "[nes] audio unavailable: %s\n", SDL_GetError());
		return;
	}
	SDL_AudioSpec want{};
	want.freq = static_cast<int>(sampleRate + 0.5);
	want.format = AUDIO_S16SYS;
	want.channels = 2;
	want.samples = 1024;
	g_audio = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
	if (!g_audio) {
		std::fprintf(stderr, "[nes] audio device failed: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return;
	}
	g_audioBytesPerSecond = static_cast<std::uint32_t>(want.freq) * 2 * sizeof(std::int16_t);
	SDL_PauseAudioDevice(g_audio, 0);
}

void closeAudio()
{
	if (!g_audio) return;
	SDL_CloseAudioDevice(g_audio);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	g_audio = 0;
}

// Integer-scales the last core frame into the top band of the canvas,
// centred and clipped to the band. Margins stay whatever Gea painted there.
bool drawFrame()
{
	const int canvasWidth = sailfish_canvas_width();
	const int bandHeight = sailfish_canvas_height() * kScreenBandPercent / 100;
	if (g_frameWidth == 0 || g_frameHeight == 0 || canvasWidth <= 0 || bandHeight <= 0) return false;
	const int frameWidth = static_cast<int>(g_frameWidth);
	const int frameHeight = static_cast<int>(g_frameHeight);
	const int scale = std::max(1, std::min(canvasWidth / frameWidth, bandHeight / frameHeight));
	const int left = (canvasWidth - frameWidth * scale) / 2;
	const int top = (bandHeight - frameHeight * scale) / 2;
	const int clipLeft = std::max(0, left);
	const int clipTop = std::max(0, top);
	const int width = std::min(canvasWidth, left + frameWidth * scale) - clipLeft;
	const int height = std::min(bandHeight, top + frameHeight * scale) - clipTop;
	if (width <= 0 || height <= 0) return false;
	g_scaled.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
	int previousSourceRow = -1;
	for (int y = 0; y < height; y++) {
		const int sourceRow = (clipTop + y - top) / scale;
		std::uint16_t *dst = g_scaled.data() + static_cast<std::size_t>(y) * width;
		if (sourceRow == previousSourceRow) {
			std::memcpy(dst, dst - width, static_cast<std::size_t>(width) * sizeof(std::uint16_t));
			continue;
		}
		const std::uint16_t *src = g_frame + static_cast<std::size_t>(sourceRow) * kMaxFrameWidth;
		for (int x = 0; x < width; x++) dst[x] = src[(clipLeft + x - left) / scale];
		previousSourceRow = sourceRow;
	}
	sailfish_display_write_rgb565(g_scaled.data(), clipLeft, clipTop, width, height);
	return true;
}

void unloadCore()
{
	retro_unload_game();
	retro_deinit();
	closeAudio();
	g_running = false;
	g_buttons = 0;
	g_frameWidth = 0;
	g_frameHeight = 0;
	g_frameChanged = false;
}

}  // namespace

namespace opengameconsole::nes {

double play()
{
	if (g_running) unloadCore();
	const unsigned char *rom = opengameconsole_pad_demo_rom;
	const std::size_t size = opengameconsole_pad_demo_rom_size;
	if (!romLooksValid(rom, size)) {
		std::fprintf(stderr, "[nes] embedded ROM is not an iNES image\n");
		return 1;
	}
	g_gameInfoExt = {};
	g_gameInfoExt.full_path = "pad-demo.nes";
	g_gameInfoExt.dir = "";
	g_gameInfoExt.name = "pad-demo";
	g_gameInfoExt.ext = "nes";
	g_gameInfoExt.data = rom;
	g_gameInfoExt.size = size;
	g_gameInfoExt.file_in_archive = false;
	g_gameInfoExt.persistent_data = true;

	retro_set_environment(environment);
	retro_set_video_refresh(videoRefresh);
	retro_set_audio_sample(audioSample);
	retro_set_audio_sample_batch(audioSampleBatch);
	retro_set_input_poll(inputPoll);
	retro_set_input_state(inputState);
	retro_init();
	retro_game_info info{};
	info.path = "pad-demo.nes";
	info.data = rom;
	info.size = size;
	if (!retro_load_game(&info)) {
		std::fprintf(stderr, "[nes] core refused the ROM\n");
		retro_deinit();
		return 2;
	}
	retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	retro_system_av_info av{};
	retro_get_system_av_info(&av);
	g_fps = av.timing.fps > 1.0 ? av.timing.fps : 60.0;
	openAudio(av.timing.sample_rate > 0 ? av.timing.sample_rate : 48000.0);
	g_buttons = 0;
	g_frameWidth = 0;
	g_frameHeight = 0;
	g_frameChanged = false;
	g_framesRun = 0;
	g_startNs = nowNs();
	g_running = true;
	return 0;
}

void setButton(const std::string &name, double down)
{
	if (!g_running) return;
	for (const ButtonName &button : kButtons) {
		if (name != button.name) continue;
		if (down != 0) g_buttons |= 1u << button.id;
		else g_buttons &= ~(1u << button.id);
		return;
	}
}

void stop()
{
	if (g_running) unloadCore();
}

}  // namespace opengameconsole::nes

// Runs the NES at its own rate on 60, 90 or 120 Hz panels: frames due since
// play() are produced, at most kMaxFramesPerLoop per loop turn; a longer stall
// skips ahead instead of fast-forwarding.
extern "C" int gea_app_before_refresh(void)
{
	if (!g_running) return 0;
	const auto due = static_cast<std::int64_t>(static_cast<double>(nowNs() - g_startNs) * g_fps / 1e9);
	for (int i = 0; i < kMaxFramesPerLoop && g_framesRun < due; i++) {
		retro_run();
		g_framesRun++;
	}
	if (g_framesRun < due) g_framesRun = due;
	if (!g_frameChanged) return 0;
	g_frameChanged = false;
	return drawFrame() ? 1 : 0;
}
