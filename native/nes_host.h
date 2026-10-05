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
