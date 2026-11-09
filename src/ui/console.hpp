#pragma once
#include "sim/world.hpp"
#include <string>
namespace curved {
struct ConsoleResult {
    bool success;
    std::string text;
    Handle selection;
};
ConsoleResult execute_console(World &world, const std::string &command);
Handle resolve_positioned(const World &world, RefId ref);
} // namespace curved
