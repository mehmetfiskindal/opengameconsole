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
