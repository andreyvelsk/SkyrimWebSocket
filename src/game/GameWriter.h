#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace GameWriter
{
    // Parses and executes a "command" message from the client.
    // Returns a JSON string suitable for sending back to the client.
    // Must be called on the game thread.
    std::string ExecuteCommand(const nlohmann::json& msg);
}
