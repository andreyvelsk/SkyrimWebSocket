#pragma once

#include <nlohmann/json.hpp>

namespace GameStatus
{
    // Returns a JSON object describing the current game / player state.
    // Lets clients tell whether the player can act right now (paused, loading,
    // dialogue, combat, controls disabled, etc.).
    // Must be called on the game thread.
    nlohmann::json ReadGameStatus();
}