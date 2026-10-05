#pragma once

#include <string>

namespace opengameconsole::nes {

// 0 started, 1 embedded ROM is not an iNES image, 2 core refused it.
double play();
// name: up, down, left, right, a, b, start, select. down: nonzero pressed.
void setButton(const std::string &name, double down);
void stop();

}  // namespace opengameconsole::nes
