#include "rom_library.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace roms = opengameconsole::roms;

namespace {

int g_failures = 0;

void check(bool ok, const char *what)
{
	if (ok) return;
	std::fprintf(stderr, "FAIL: %s\n", what);
	g_failures++;
}

void writeFile(const std::string &path, std::size_t size)
{
	std::FILE *file = std::fopen(path.c_str(), "wb");
	if (!file) return;
	const std::vector<unsigned char> bytes(size, 0x42);
	if (size) std::fwrite(bytes.data(), 1, size, file);
	std::fclose(file);
}

bool isDirectory(const std::string &path)
{
	struct stat st{};
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

}  // namespace

int main()
{
	char pattern[] = "/tmp/rom_library_test.XXXXXX";
	const char *made = mkdtemp(pattern);
	if (!made) {
		std::perror("mkdtemp");
		return 2;
	}
	const std::string home = made;

	check(roms::scan("").empty(), "empty home lists nothing");
	check(roms::scan(home).empty(), "fresh home lists nothing");
	check(isDirectory(home + "/Documents/NES"), "scan creates Documents/NES");

	mkdir((home + "/Downloads").c_str(), 0755);
	mkdir((home + "/Documents/NES/sub").c_str(), 0755);
	mkdir((home + "/Downloads/folder.nes").c_str(), 0755);
	writeFile(home + "/Documents/NES/zelda-like.nes", 32);
	writeFile(home + "/Documents/NES/Alter Ego.NES", 32);
	writeFile(home + "/Documents/NES/notes.txt", 32);
	writeFile(home + "/Documents/NES/sub/hidden.nes", 32);
	writeFile(home + "/Downloads/alter ego.nes", 32);
	writeFile(home + "/Downloads/blade.nes", 32);

	const std::vector<roms::RomFile> files = roms::scan(home);
	check(files.size() == 4, "four roms found, txt, subfolder and directory skipped");
	if (files.size() == 4) {
		check(files[0].name == "Alter Ego" && files[0].folder == roms::Folder::Documents, "Documents copy sorts first");
		check(files[1].name == "alter ego (Downloads)" && files[1].stem == "alter ego", "Downloads duplicate gets suffix");
		check(files[2].name == "blade" && files[2].path == home + "/Downloads/blade.nes", "blade path");
		check(files[3].name == "zelda-like" && files[3].folder == roms::Folder::Documents, "zelda-like last");
	}

	std::vector<unsigned char> rom;
	check(roms::readRomFile(home + "/Downloads/blade.nes", rom) && rom.size() == 32, "reads a rom");
	writeFile(home + "/Downloads/huge.nes", roms::kMaxRomBytes + 1);
	check(!roms::readRomFile(home + "/Downloads/huge.nes", rom), "rejects over 4 MiB");
	writeFile(home + "/Downloads/empty.nes", 0);
	check(!roms::readRomFile(home + "/Downloads/empty.nes", rom), "rejects empty file");
	check(!roms::readRomFile(home + "/Downloads/missing.nes", rom), "rejects missing file");

	const unsigned char digits[] = "123456789";
	check(roms::crc32(digits, 9) == 0xCBF43926u, "crc32 check value");
	check(roms::saveFileName("Alter Ego: \xC5\x9E", digits, 9) == "Alter Ego_ __-cbf43926.sav", "save file name");

	check(roms::saveDirectory("").empty(), "no home, no saves");
	check(roms::saveDirectory("/home/u") == "/home/u/.local/share/org.opengameconsole/opengameconsole/saves", "save directory");

	const std::string save = roms::saveDirectory(home) + "/game-cbf43926.sav";
	const unsigned char sram[4] = {1, 2, 3, 4};
	check(roms::writeSave(save, sram, 4), "writes a save and creates its folder");
	std::vector<unsigned char> loaded;
	check(roms::readSave(save, 4, loaded) && loaded == std::vector<unsigned char>(sram, sram + 4), "reads the save back");
	check(!roms::readSave(save, 8, loaded), "rejects a save of the wrong size");
	check(access((save + ".tmp").c_str(), F_OK) != 0, "no temp file left behind");
	const unsigned char next[4] = {9, 9, 9, 9};
	check(roms::writeSave(save, next, 4) && roms::readSave(save, 4, loaded) && loaded[0] == 9, "overwrites a save");

	const std::string cleanup = "rm -rf '" + home + "'";
	const int removed = std::system(cleanup.c_str());
	(void)removed;
	if (g_failures == 0) std::puts("rom_library tests passed");
	return g_failures == 0 ? 0 : 1;
}
