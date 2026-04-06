#pragma once

#include <string>

namespace GameWriter
{
    // Must be called on the game thread
    void ApplyClientCommand(const std::string& json);
}
