# NES ROM kütüphanesi Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Kabukta `~/Documents/NES` ve `~/Downloads` içindeki `.nes` dosyalarını listeleyip açmak; SRAM kayıtlarını uygulama veri klasöründe saklamak.

**Architecture:** Yeni `native/rom_library.{h,cpp}` dosya tarama, ROM okuma ve kayıt dosyalarını yapar; SDL, libretro ve Gea’ya bağlı değildir ve Sailfish SDK derleme kabuğunda test edilir. `native/nes_host.cpp` listeyi tutar (başta gömülü Pad Demo), girişi yükler, SRAM’ı okur/yazar. TSX kabuğu listeyi bir `ReactiveComponent` alanındaki nesne dizisi olarak tutar ve `{this.roms.map(...)}` ile çizer.

**Tech Stack:** C++17 (POSIX `dirent`, `stat`, `rename`), libretro FCEUmm, GeaStack TSX → geatsc C++, Sailfish SDK `sfdk` (i486 test, aarch64 RPM), Node `node:test`.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-10-05-nes-rom-library-design.md`.
- Klasörler: `~/Documents/NES` ve `~/Downloads`. Alt klasörlere inilmez.
- Dosya: adı `.nes` ile biten (büyük/küçük harf farketmez) düz dosya. En çok 4 MiB.
- Kayıt yeri: `~/.local/share/org.opengameconsole/opengameconsole/saves/`, dosya adı `<güvenli-kök>-<crc32>.sav`.
- İzinler: `["Audio", "Documents", "Downloads"]`.
- Hata kodları: 0 başladı, 1 NES ROM’u değil, 2 çekirdek başlamadı, 3 okunamadı / 4 MiB üstü / aralık dışı.
- Hata metinleri: 1 “Bu dosya bir NES ROM’u değil”, 2 “NES başlamadı”, 3 “Dosya okunamadı”.
- İpucu metni: “ROM’ları ~/Documents/NES veya ~/Downloads klasörüne koy”.
- RPM içinde pad demo dışında ROM yoktur.
- `vendor/geastack-linux` bir git deposu değildir; bu plan oraya dokunmaz.
- Kabuk PowerShell’dir: `&&` yok, `;` ve `$LASTEXITCODE` kullanılır.
- Parola hiçbir dosyaya, betiğe veya commit’e yazılmaz.

## Doğrulanmış GeaStack davranışları

Bu plan yazılırken `-PrepareOnly` ile denendi:

- `ReactiveComponent` üzerindeki `roms: RomItem[]` alanı reaktif liste kaynağıdır; `{this.roms.map((rom) => <button key=...>)}` uyarısız C++’a iner.
- Alan başlatıcısında host çağrısı (`roms: RomItem[] = loadRoms()`) ve `try/catch` desteklenir.
- `nesRomName` C++’ta `std::string` döndürmelidir; sıra numaraları `double` gelir.
- `created`/`onAfterRender` kullanılmaz: bileşeni yavaş uyumluluk yoluna iter.

---

### Task 1: `rom_library` ve native test

**Files:**
- Create: `native/rom_library.h`
- Create: `native/rom_library.cpp`
- Create: `native/tests/rom_library_test.cpp`
- Create: `scripts/test-native.ps1`
- Modify: `package.json` (`scripts.test:native`)

**Interfaces:**
- Produces (namespace `opengameconsole::roms`):
  - `constexpr std::size_t kMaxRomBytes = 4u * 1024u * 1024u;`
  - `enum class Folder { Documents, Downloads };`
  - `struct RomFile { std::string name; std::string stem; std::string path; Folder folder; };`
  - `std::vector<RomFile> scan(const std::string &home);`
  - `bool readRomFile(const std::string &path, std::vector<unsigned char> &out);`
  - `std::uint32_t crc32(const unsigned char *data, std::size_t size);`
  - `std::string saveFileName(const std::string &stem, const unsigned char *rom, std::size_t size);`
  - `std::string saveDirectory(const std::string &home);`
  - `bool readSave(const std::string &path, std::size_t size, std::vector<unsigned char> &out);`
  - `bool writeSave(const std::string &path, const unsigned char *data, std::size_t size);`
  - `npm run test:native` → çıkış 0 ve `rom_library tests passed`.

- [ ] **Step 1: Başarısız testi yaz**

`native/tests/rom_library_test.cpp`:

```cpp
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
```

`scripts/test-native.ps1`:

```powershell
param(
	[string]$SdkRoot = 'C:\SailfishOS',
	[string]$Target = 'SailfishOS-5.1.0.11-i486'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$work = Join-Path $root '.gea-sailfish\native-tests'
New-Item -ItemType Directory -Force $work | Out-Null
Copy-Item -Force -Path (Join-Path $root 'native\rom_library.cpp'), (Join-Path $root 'native\rom_library.h'), (Join-Path $root 'native\tests\rom_library_test.cpp') -Destination $work
$sfdk = Join-Path $SdkRoot 'bin\sfdk.exe'
if (!(Test-Path $sfdk)) { throw "Sailfish SDK missing at $sfdk" }
$config = "target=$Target"
# sfdk writes progress to stderr; with Stop that would abort the script.
$ErrorActionPreference = 'Continue'
Push-Location $work
try {
	& $sfdk --no-session -c $config build-init 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -notmatch 'Already initialized' }
	& $sfdk --no-session -c $config build-shell g++ -std=c++17 -Wall -Wextra -Werror -O1 -o rom_library_test rom_library_test.cpp rom_library.cpp 2>&1 | ForEach-Object { "$_" }
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	& $sfdk --no-session -c $config build-shell ./rom_library_test 2>&1 | ForEach-Object { "$_" }
	exit $LASTEXITCODE
} finally {
	Pop-Location
}
```

`package.json` `scripts` içine, `"rom"` satırından sonra:

```json
    "test:native": "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-native.ps1",
```

- [ ] **Step 2: Testin başarısız olduğunu gör**

Run: `npm run test:native`
Expected: FAIL, `Copy-Item` “Cannot find path ...\native\rom_library.cpp” hatası.

- [ ] **Step 3: `native/rom_library.h` yaz**

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opengameconsole::roms {

constexpr std::size_t kMaxRomBytes = 4u * 1024u * 1024u;

enum class Folder { Documents, Downloads };

struct RomFile {
	std::string name;
	std::string stem;
	std::string path;
	Folder folder;
};

// *.nes files directly inside home/Documents/NES and home/Downloads, sorted by
// name ignoring case. Creates home/Documents/NES when missing. Empty home: none.
std::vector<RomFile> scan(const std::string &home);
// False for missing, empty or over kMaxRomBytes files.
bool readRomFile(const std::string &path, std::vector<unsigned char> &out);
std::uint32_t crc32(const unsigned char *data, std::size_t size);
// <stem with unsafe bytes as '_'>-<crc32 of rom, 8 lowercase hex>.sav
std::string saveFileName(const std::string &stem, const unsigned char *rom, std::size_t size);
// Empty when home is empty.
std::string saveDirectory(const std::string &home);
// False unless the file exists and is exactly size bytes.
bool readSave(const std::string &path, std::size_t size, std::vector<unsigned char> &out);
// Writes path.tmp, then renames it over path, creating the folder first.
bool writeSave(const std::string &path, const unsigned char *data, std::size_t size);

}  // namespace opengameconsole::roms
```

- [ ] **Step 4: `native/rom_library.cpp` yaz**

```cpp
#include "rom_library.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>

namespace opengameconsole::roms {

namespace {

std::string lower(std::string text)
{
	for (char &c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

bool hasNesExtension(const std::string &file)
{
	return file.size() > 4 && lower(file.substr(file.size() - 4)) == ".nes";
}

bool regularFileSize(const std::string &path, unsigned long long &size)
{
	struct stat st{};
	if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return false;
	size = static_cast<unsigned long long>(st.st_size);
	return true;
}

void makeDirectories(const std::string &path)
{
	for (std::size_t i = 1; i <= path.size(); i++) {
		if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755);
	}
}

void collect(const std::string &dir, Folder folder, std::vector<RomFile> &out)
{
	DIR *handle = opendir(dir.c_str());
	if (!handle) return;
	while (const dirent *item = readdir(handle)) {
		const std::string file = item->d_name;
		if (!hasNesExtension(file)) continue;
		const std::string path = dir + "/" + file;
		unsigned long long size = 0;
		if (!regularFileSize(path, size)) continue;
		const std::string stem = file.substr(0, file.size() - 4);
		out.push_back({stem, stem, path, folder});
	}
	closedir(handle);
}

bool readWhole(const std::string &path, std::size_t size, std::vector<unsigned char> &out)
{
	std::FILE *file = std::fopen(path.c_str(), "rb");
	if (!file) return false;
	out.resize(size);
	const bool ok = std::fread(out.data(), 1, size, file) == size;
	std::fclose(file);
	if (!ok) out.clear();
	return ok;
}

}  // namespace

std::vector<RomFile> scan(const std::string &home)
{
	std::vector<RomFile> files;
	if (home.empty()) return files;
	const std::string documents = home + "/Documents/NES";
	makeDirectories(documents);
	collect(documents, Folder::Documents, files);
	collect(home + "/Downloads", Folder::Downloads, files);
	std::sort(files.begin(), files.end(), [](const RomFile &a, const RomFile &b) {
		const std::string la = lower(a.name);
		const std::string lb = lower(b.name);
		if (la != lb) return la < lb;
		if (a.folder != b.folder) return a.folder == Folder::Documents;
		return a.name < b.name;
	});
	std::set<std::string> documentNames;
	for (const RomFile &file : files) {
		if (file.folder == Folder::Documents) documentNames.insert(lower(file.stem));
	}
	for (RomFile &file : files) {
		if (file.folder == Folder::Downloads && documentNames.count(lower(file.stem))) file.name += " (Downloads)";
	}
	return files;
}

bool readRomFile(const std::string &path, std::vector<unsigned char> &out)
{
	unsigned long long size = 0;
	if (!regularFileSize(path, size) || size == 0 || size > kMaxRomBytes) return false;
	return readWhole(path, static_cast<std::size_t>(size), out);
}

std::uint32_t crc32(const unsigned char *data, std::size_t size)
{
	std::uint32_t crc = 0xFFFFFFFFu;
	for (std::size_t i = 0; i < size; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
	}
	return ~crc;
}

std::string saveFileName(const std::string &stem, const unsigned char *rom, std::size_t size)
{
	std::string safe;
	for (char c : stem) {
		const unsigned char u = static_cast<unsigned char>(c);
		const bool keep = (u < 128 && std::isalnum(u)) || c == '.' || c == '_' || c == '-' || c == ' ';
		safe += keep ? c : '_';
	}
	char crc[9];
	std::snprintf(crc, sizeof crc, "%08x", static_cast<unsigned>(crc32(rom, size)));
	return safe + "-" + crc + ".sav";
}

std::string saveDirectory(const std::string &home)
{
	if (home.empty()) return "";
	return home + "/.local/share/org.opengameconsole/opengameconsole/saves";
}

bool readSave(const std::string &path, std::size_t size, std::vector<unsigned char> &out)
{
	unsigned long long actual = 0;
	if (size == 0 || !regularFileSize(path, actual) || actual != size) return false;
	return readWhole(path, size, out);
}

bool writeSave(const std::string &path, const unsigned char *data, std::size_t size)
{
	const std::size_t slash = path.rfind('/');
	if (slash != std::string::npos && slash > 0) makeDirectories(path.substr(0, slash));
	const std::string temp = path + ".tmp";
	std::FILE *file = std::fopen(temp.c_str(), "wb");
	if (!file) return false;
	bool ok = std::fwrite(data, 1, size, file) == size;
	ok = std::fflush(file) == 0 && ok;
	ok = fsync(fileno(file)) == 0 && ok;
	ok = std::fclose(file) == 0 && ok;
	if (!ok || std::rename(temp.c_str(), path.c_str()) != 0) {
		std::remove(temp.c_str());
		return false;
	}
	return true;
}

}  // namespace opengameconsole::roms
```

- [ ] **Step 5: Testin geçtiğini gör**

Run: `npm run test:native`
Expected: çıkış 0, son satır `rom_library tests passed`, hiç `FAIL:` satırı yok.

- [ ] **Step 6: Diğer testler hâlâ geçiyor mu**

Run: `npm test; npm run check`
Expected: ikisi de çıkış 0.

- [ ] **Step 7: Commit**

```powershell
git add native/rom_library.h native/rom_library.cpp native/tests/rom_library_test.cpp scripts/test-native.ps1 package.json
git commit -m "feat: add ROM folder scan and SRAM save files"
```

---

### Task 2: `nes_host` listeyi, girişleri ve SRAM’ı yönetir

**Files:**
- Modify: `native/nes_host.h` (tamamı)
- Modify: `native/nes_host.cpp` (include’lar, durum, `unloadCore`, `opengameconsole::nes` fonksiyonları, `gea_app_before_refresh`)
- Modify: `scripts/nes-host-plugin.mjs` (`hostFunctions`)
- Modify: `scripts/nes-host-plugin.test.mjs`
- Modify: `package.json` (`gea.sailfish.nativeSources`, `gea.sailfish.permissions`)

**Interfaces:**
- Consumes: Task 1’in `opengameconsole::roms` API’si.
- Produces (namespace `opengameconsole::nes`):
  - `double refreshRoms();` — giriş sayısı, en az 1; giriş 0 Pad Demo.
  - `std::string romName(double index);` — aralık dışında `""`.
  - `double play(double index);` — 0/1/2/3.
  - `void setButton(const std::string &name, double down);` — değişmez.
  - `void stop();` — önce SRAM yazar.
- Produces (geatsc): `nesRefreshRoms`, `nesRomName`, `nesPlay`, `nesSetButton`, `nesStop` bu sırayla.

Bu görevden sonra TSX hâlâ `nesPlay()` argümansız çağırır; tam RPM derlemesi Task 3 sonunda yapılır.

- [ ] **Step 1: Eklenti testini güncelle (başarısız)**

`scripts/nes-host-plugin.test.mjs` içindeki testi şununla değiştir:

```js
test('geatsc accepts the nes host plugin and maps the five calls', async () => {
  const plugins = await loadCliPlugins([pluginPath])
  const plugin = plugins.find((item) => item.name === 'opengameconsole-nes-host')
  assert.ok(plugin, 'plugin loaded')
  const { capabilities } = plugin.instantiate(new Map())
  assert.deepEqual([...capabilities.hostFunctions], [
    ['nesRefreshRoms', 'opengameconsole::nes::refreshRoms'],
    ['nesRomName', 'opengameconsole::nes::romName'],
    ['nesPlay', 'opengameconsole::nes::play'],
    ['nesSetButton', 'opengameconsole::nes::setButton'],
    ['nesStop', 'opengameconsole::nes::stop'],
  ])
  for (const spelling of capabilities.hostFunctions.values()) {
    assert.deepEqual(capabilities.hostPreambles.get(spelling), ['#include "nes_host.h"'])
  }
})
```

- [ ] **Step 2: Testin başarısız olduğunu gör**

Run: `npm test`
Expected: FAIL, `deepEqual` farkında `nesRefreshRoms` eksik.

- [ ] **Step 3: Eklentiyi güncelle**

`scripts/nes-host-plugin.mjs` içinde `hostFunctions`:

```js
const hostFunctions = [
  ['nesRefreshRoms', 'opengameconsole::nes::refreshRoms'],
  ['nesRomName', 'opengameconsole::nes::romName'],
  ['nesPlay', 'opengameconsole::nes::play'],
  ['nesSetButton', 'opengameconsole::nes::setButton'],
  ['nesStop', 'opengameconsole::nes::stop'],
]
```

Run: `npm test`
Expected: PASS.

- [ ] **Step 4: `native/nes_host.h` tamamını değiştir**

```cpp
#pragma once

#include <string>

namespace opengameconsole::nes {

// Rescans ~/Documents/NES and ~/Downloads. Returns the entry count; entry 0 is
// the embedded Pad Demo, so the count is at least 1.
double refreshRoms();
// Display name of an entry, empty when out of range.
std::string romName(double index);
// 0 started, 1 not an iNES image, 2 core refused it,
// 3 unreadable, over 4 MiB or no such entry.
double play(double index);
// name: up, down, left, right, a, b, start, select. down: nonzero pressed.
void setButton(const std::string &name, double down);
void stop();

}  // namespace opengameconsole::nes
```

- [ ] **Step 5: `nes_host.cpp` include’ları ve sabitler**

`#include "nes_host.h"` altına:

```cpp
#include "rom_library.h"
```

`#include <cstring>` altına:

```cpp
#include <cstdlib>
#include <string>
```

`constexpr std::uint32_t kAudioQueueLimitMs = 120;` altına:

```cpp
constexpr std::int64_t kSaveIntervalNs = 30LL * 1000000000LL;
```

- [ ] **Step 6: `nes_host.cpp` durum alanları**

`retro_game_info_ext g_gameInfoExt{};` satırının altına:

```cpp
struct LibraryEntry {
	std::string name;
	std::string stem;
	std::string path;
};

std::vector<LibraryEntry> g_library;
std::vector<unsigned char> g_rom;
std::string g_romPath;
std::string g_romDir;
std::string g_romStem;
std::string g_savePath;
std::vector<unsigned char> g_savedSram;
std::int64_t g_lastSaveCheckNs = 0;
```

- [ ] **Step 7: `nes_host.cpp` yardımcılar ve `unloadCore`**

Mevcut `unloadCore` fonksiyonunu tamamen şununla değiştir (önündeki üç yardımcıyla birlikte):

```cpp
std::string homeDirectory()
{
	const char *home = std::getenv("HOME");
	return home ? home : "";
}

void loadSave()
{
	g_savePath.clear();
	g_savedSram.clear();
	const std::size_t size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
	auto *data = static_cast<unsigned char *>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
	const std::string dir = opengameconsole::roms::saveDirectory(homeDirectory());
	if (size == 0 || !data || dir.empty()) return;
	g_savePath = dir + "/" + opengameconsole::roms::saveFileName(g_romStem, g_rom.data(), g_rom.size());
	if (opengameconsole::roms::readSave(g_savePath, size, g_savedSram)) std::memcpy(data, g_savedSram.data(), size);
	else g_savedSram.assign(data, data + size);
}

// Writes SRAM only when it differs from what was last read or written.
void flushSave()
{
	if (g_savePath.empty()) return;
	const std::size_t size = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
	const auto *data = static_cast<const unsigned char *>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
	if (size == 0 || !data) return;
	if (g_savedSram.size() == size && std::memcmp(g_savedSram.data(), data, size) == 0) return;
	if (!opengameconsole::roms::writeSave(g_savePath, data, size)) {
		std::fprintf(stderr, "[nes] could not write %s\n", g_savePath.c_str());
		return;
	}
	g_savedSram.assign(data, data + size);
}

void unloadCore()
{
	flushSave();
	retro_unload_game();
	retro_deinit();
	closeAudio();
	g_running = false;
	g_buttons = 0;
	g_frameWidth = 0;
	g_frameHeight = 0;
	g_frameChanged = false;
	g_savePath.clear();
	g_savedSram.clear();
	g_rom.clear();
}
```

- [ ] **Step 8: `nes_host.cpp` `refreshRoms`, `romName`, `play`**

`namespace opengameconsole::nes {` içindeki mevcut `play()` fonksiyonunu tamamen şununla değiştir (`setButton` ve `stop` olduğu gibi kalır):

```cpp
double refreshRoms()
{
	g_library.clear();
	g_library.push_back({"Pad Demo", "pad-demo", ""});
	for (const opengameconsole::roms::RomFile &file : opengameconsole::roms::scan(homeDirectory()))
		g_library.push_back({file.name, file.stem, file.path});
	return static_cast<double>(g_library.size());
}

std::string romName(double index)
{
	if (!(index >= 0) || index >= static_cast<double>(g_library.size())) return "";
	return g_library[static_cast<std::size_t>(index)].name;
}

double play(double index)
{
	if (g_running) unloadCore();
	if (g_library.empty()) refreshRoms();
	if (!(index >= 0) || index >= static_cast<double>(g_library.size())) return 3;
	const LibraryEntry entry = g_library[static_cast<std::size_t>(index)];
	if (entry.path.empty()) {
		g_rom.assign(opengameconsole_pad_demo_rom, opengameconsole_pad_demo_rom + opengameconsole_pad_demo_rom_size);
		g_romPath = "pad-demo.nes";
		g_romDir = "";
	} else {
		if (!opengameconsole::roms::readRomFile(entry.path, g_rom)) {
			std::fprintf(stderr, "[nes] cannot read %s\n", entry.path.c_str());
			return 3;
		}
		g_romPath = entry.path;
		g_romDir = entry.path.substr(0, entry.path.rfind('/'));
	}
	g_romStem = entry.stem;
	if (!romLooksValid(g_rom.data(), g_rom.size())) {
		std::fprintf(stderr, "[nes] %s is not an iNES image\n", g_romPath.c_str());
		g_rom.clear();
		return 1;
	}
	g_gameInfoExt = {};
	g_gameInfoExt.full_path = g_romPath.c_str();
	g_gameInfoExt.dir = g_romDir.c_str();
	g_gameInfoExt.name = g_romStem.c_str();
	g_gameInfoExt.ext = "nes";
	g_gameInfoExt.data = g_rom.data();
	g_gameInfoExt.size = g_rom.size();
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
	info.path = g_romPath.c_str();
	info.data = g_rom.data();
	info.size = g_rom.size();
	if (!retro_load_game(&info)) {
		std::fprintf(stderr, "[nes] core refused %s\n", g_romPath.c_str());
		retro_deinit();
		g_rom.clear();
		return 2;
	}
	retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
	loadSave();
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
	g_lastSaveCheckNs = g_startNs;
	g_running = true;
	return 0;
}
```

- [ ] **Step 9: `gea_app_before_refresh` içinde 30 saniyelik kayıt**

Fonksiyonun gövdesini şununla değiştir (üstündeki yorum kalır):

```cpp
{
	if (!g_running) return 0;
	const std::int64_t now = nowNs();
	const auto due = static_cast<std::int64_t>(static_cast<double>(now - g_startNs) * g_fps / 1e9);
	for (int i = 0; i < kMaxFramesPerLoop && g_framesRun < due; i++) {
		retro_run();
		g_framesRun++;
	}
	if (g_framesRun < due) g_framesRun = due;
	if (now - g_lastSaveCheckNs >= kSaveIntervalNs) {
		g_lastSaveCheckNs = now;
		flushSave();
	}
	if (!g_frameChanged) return 0;
	g_frameChanged = false;
	return drawFrame() ? 1 : 0;
}
```

- [ ] **Step 10: `package.json` native kaynaklar ve izinler**

`gea.sailfish` içinde:

```json
      "permissions": ["Audio", "Documents", "Downloads"],
      "nativeSources": ["native/nes_host.cpp", "native/nes_host.h", "native/rom_library.cpp", "native/rom_library.h", "native/pad_demo_rom.c"],
```

- [ ] **Step 11: Gözden geçir ve testler**

`native/nes_host.cpp` içinde `opengameconsole_pad_demo_rom` yalnızca `play` içinde, `g_gameInfoExt.data` ve `info.data` yalnızca `g_rom.data()` olmalı:

Run: `rg -n "opengameconsole_pad_demo_rom|\.data = " native/nes_host.cpp`
Expected: `extern` bildirimleri, `g_rom.assign(...)` satırı, `g_gameInfoExt.data = g_rom.data();` ve `info.data = g_rom.data();`.

Run: `npm test; npm run test:native`
Expected: ikisi de çıkış 0.

- [ ] **Step 12: Commit**

```powershell
git add native/nes_host.h native/nes_host.cpp scripts/nes-host-plugin.mjs scripts/nes-host-plugin.test.mjs package.json
git commit -m "feat: play ROMs from the library and keep SRAM saves"
```

---

### Task 3: Kabukta oyun listesi ve aarch64 RPM

**Files:**
- Modify: `src/nes-host.d.ts` (tamamı)
- Modify: `src/index.tsx` (tamamı)
- Modify: `src/styles.css` (`.shell` … `.play` arası)

**Interfaces:**
- Consumes: Task 2’nin beş host çağrısı.
- Produces: `harbour-gea-opengameconsole-0.1.0-1.aarch64.rpm`, Documents/Downloads izinli.

- [ ] **Step 1: `src/nes-host.d.ts` tamamını değiştir**

```ts
declare function nesRefreshRoms(): number
declare function nesRomName(index: number): string
declare function nesPlay(index: number): number
declare function nesSetButton(name: string, down: number): void
declare function nesStop(): void
```

Run: `npm run check`
Expected: FAIL, `src/index.tsx` içinde `nesPlay()` için “Expected 1 arguments, but got 0”.

- [ ] **Step 2: `src/index.tsx` kabuk kısmı**

Dosyanın başından `template() {` satırına kadar olan kısmı şununla değiştir:

```tsx
import { ReactiveComponent, mount } from '@geastack/core'
import './styles.css'

interface RomItem {
  key: string
  index: number
  name: string
}

function playError(code: number): string {
  if (code === 1) return 'Bu dosya bir NES ROM’u değil'
  if (code === 2) return 'NES başlamadı'
  if (code === 3) return 'Dosya okunamadı'
  return 'Bilinmeyen hata'
}

// Host calls exist only in the native build; in the browser the list stays empty.
function loadRoms(): RomItem[] {
  const items: RomItem[] = []
  try {
    const count = nesRefreshRoms()
    for (let i = 0; i < count; i++) {
      const name = nesRomName(i)
      items.push({ key: i + '/' + name, index: i, name })
    }
  } catch (error) {
    return items
  }
  return items
}

export class App extends ReactiveComponent {
  screen = 'shell'
  message = ''
  roms: RomItem[] = loadRoms()

  refresh() {
    this.message = ''
    this.roms = loadRoms()
  }

  play(index: number) {
    const code = nesPlay(index)
    if (code === 0) {
      this.message = ''
      this.screen = 'game'
    } else {
      this.message = playError(code)
    }
  }

  back() {
    nesStop()
    this.screen = 'shell'
    this.roms = loadRoms()
  }

```

Şablonda kabuk `div`’ini (`<div class={this.screen === 'shell' ? 'shell' : 'hidden'}>` ile başlayan ve `<span class="message">` sonrası kapanan blok) şununla değiştir; oyun `div`’i aynen kalır:

```tsx
        <div class={this.screen === 'shell' ? 'shell' : 'hidden'}>
          <div class="shell-header">
            <span class="title">Opengameconsole</span>
            <button class="refresh" onClick={() => this.refresh()}>Yenile</button>
          </div>
          <div class="rom-list">
            {this.roms.map((rom) => (
              <button key={rom.key} class="rom" onClick={() => this.play(rom.index)}>{rom.name}</button>
            ))}
          </div>
          <span class={this.roms.length > 1 ? 'hidden' : 'hint'}>ROM’ları ~/Documents/NES veya ~/Downloads klasörüne koy</span>
          <span class="message">{this.message}</span>
        </div>
```

- [ ] **Step 3: `src/styles.css` kabuk stilleri**

`.shell {` bloğundan `.play { ... }` bloğunun sonuna kadar olan kısmı şununla değiştir:

```css
.shell {
  display: flex;
  flex-direction: column;
  flex: 1;
  gap: 12px;
  padding: 16px;
  background-color: #101418;
}

.shell-header {
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: space-between;
}

.title { color: #2dd4bf; font-size: 28px; }
.hint { color: #94a3b8; font-size: 16px; }
.message { color: #f87171; font-size: 18px; }

.refresh {
  width: 104px;
  height: 40px;
  border-radius: 10px;
  background-color: #334155;
  color: #f8fafc;
  font-size: 18px;
}

.rom-list {
  display: flex;
  flex-direction: column;
  flex: 1;
  min-height: 0;
  gap: 8px;
  overflow-y: auto;
}

.rom {
  height: 56px;
  flex-shrink: 0;
  border-radius: 12px;
  background-color: #1f2a33;
  color: #f8fafc;
  font-size: 20px;
}
```

- [ ] **Step 4: tsc, testler ve web build**

Run: `npm run check; npm test; npm run build`
Expected: üçü de çıkış 0.

- [ ] **Step 5: C++ üretimini ve bağlamaları kontrol et**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File node_modules/@geastack/linux/targets/sailfish-os/build-sailfish-os.ps1 -AppDirectory . -Architecture aarch64 -PrepareOnly *> .gea-sailfish/prepare.log
Select-String -Path .gea-sailfish/prepare.log -Pattern 'gea-nonreactive-list|error'
$gen = '.gea-sailfish/build/opengameconsole-aarch64/project/generated/index.cpp'
foreach ($name in 'refreshRoms','romName','play','setButton','stop') { $pattern = 'opengameconsole::nes::' + $name + '\('; $count = (Select-String -Path $gen -Pattern $pattern).Count; "$name $count" }
Select-String -Path .gea-sailfish/build/opengameconsole-aarch64/project/*.desktop -Pattern 'Permissions='
```

Expected: ilk `Select-String` yalnızca PowerShell’in stderr sarmalayıcısı olan `NativeCommandError` satırını gösterebilir; başka satır yok. `refreshRoms` ve `romName` en az 1 (alan başlatıcısı ve `refresh`/`back` aynı `loadRoms` gövdesini paylaşabilir), `play` 1, `setButton` 16, `stop` 1. `Permissions=Audio;Documents;Downloads`.

- [ ] **Step 6: aarch64 RPM**

Run: `npm run sailfish:build *> .gea-sailfish/sfdk-build.log; $LASTEXITCODE`
Expected: `0`; `.gea-sailfish/build/opengameconsole-aarch64/project/RPMS/harbour-gea-opengameconsole-0.1.0-1.aarch64.rpm` yeni zaman damgalı. Başarısızsa ilk `error:` satırı: `Select-String -Path .gea-sailfish/sfdk-build.log -Pattern 'error:' | Select-Object -First 5`.

- [ ] **Step 7: Commit**

```powershell
git add src/nes-host.d.ts src/index.tsx src/styles.css
git commit -m "feat: list and open ROMs from the shell"
```

---

### Task 4: Telefonda kurulum ve deneme (kullanıcı)

**Files:** yok.

**Interfaces:**
- Consumes: Task 3’ün RPM’i, `scripts/deploy-sailfish.ps1` (`-PhoneHost` parametresi).

Telefon USB geliştirici modunda `192.168.2.15` adresindedir. SSH parolası etkileşimli sorulduğu için komutları kullanıcı kendi terminalinde çalıştırır.

- [ ] **Step 1: Kur**

Run (kullanıcı): `npm run sailfish:deploy -- -PhoneHost 192.168.2.15`
Expected: `scp` aktarır, `pkcon` “Finished”. Yetki hatası olursa kullanıcıya sorulur; `devel-su` yalnızca onayla eklenir.

- [ ] **Step 2: Bir homebrew ROM kopyala**

Lisansı serbest bir `.nes` dosyası (ör. NESdev veya itch.io’dan ücretsiz bir homebrew):

Run (kullanıcı): `scp "<dosya>.nes" defaultuser@192.168.2.15:Documents/NES/`

- [ ] **Step 3: Telefonda deneme**

1. Uygulama açılır; listede “Pad Demo” ve kopyalanan oyun var. Yalnızca Pad Demo varken ipucu görünür, oyun eklenince kaybolur.
2. Pad Demo eskisi gibi oynanır (kare, renkler, Geri).
3. Homebrew oyun açılır, görüntü ve ses gelir, tuşlar çalışır.
4. Oyun açıkken `~/Downloads` içine başka bir `.nes` kopyalanır; Geri’den sonra liste onu gösterir. Yenile de aynı işi yapar.
5. `.txt` dosyasının adını `.nes` yapıp koymak listede görünür; açınca “Bu dosya bir NES ROM’u değil”.
6. Liste ekrandan uzunsa parmakla kaydırılır.
7. SRAM kullanan bir oyun varsa: ilerleme kaydedilir, Geri, oyun yeniden açılır, ilerleme durur. `ls ~/.local/share/org.opengameconsole/opengameconsole/saves` bir `.sav` gösterir.

Bir adım başarısızsa log:

```powershell
ssh defaultuser@192.168.2.15 "journalctl --user -b --no-pager | grep -E 'nes|sailfish' | tail -n 80"
```
