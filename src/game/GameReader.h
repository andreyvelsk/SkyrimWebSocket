#pragma once

#include <string>

namespace GameReader
{
    // Must be called on the game thread
    std::string BuildPlayerStateJson();
}
