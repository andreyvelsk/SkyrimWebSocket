#pragma once

#include <nlohmann/json.hpp>

namespace PlayerReader
{
    // Returns the player's current character level as an integer.
    // Must be called on the game thread.
    nlohmann::json ReadLevel();

    // Returns the player's current XP within the current level as a float.
    // Must be called on the game thread.
    nlohmann::json ReadXPCurrent();

    // Returns the XP threshold required to reach the next level as a float.
    // Must be called on the game thread.
    nlohmann::json ReadXPNext();

    // Returns the XP value at the start of the current level as a float (always 0.0).
    // Provided for progress-bar calculations alongside XPCurrent and XPNext.
    // Must be called on the game thread.
    nlohmann::json ReadXPLevelStart();

    // Returns the total weight of all items currently in the player's inventory as a float.
    // Must be called on the game thread.
    nlohmann::json ReadInventoryWeight();

    // Returns the player's maximum carry weight as a float.
    // Must be called on the game thread.
    nlohmann::json ReadCarryWeight();
}
