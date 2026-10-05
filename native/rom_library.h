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
