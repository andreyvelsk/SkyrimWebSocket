#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace InventoryWriter
{
    // Equip an item from the player's inventory.
    // hand: "right" (default) or "left" — only meaningful for weapons.
    // Must be called on the game thread.
    nlohmann::json Equip(std::uint32_t formId, const std::string& hand);

    // Unequip an item from the player.
    // hand: "right", "left", or empty/other — empty/other unequips from all slots.
    // Must be called on the game thread.
    nlohmann::json Unequip(std::uint32_t formId, const std::string& hand);

    // Use/consume an item (potions, food, scrolls).
    // Must be called on the game thread.
    nlohmann::json Use(std::uint32_t formId);

    // Drop `count` copies of the item from the player's inventory.
    // count is clamped to [1, available quantity].
    // Must be called on the game thread.
    nlohmann::json Drop(std::uint32_t formId, int count);

    // Add (add=true) or remove (add=false) an item from the player's favorites.
    // Must be called on the game thread.
    nlohmann::json Favorite(std::uint32_t formId, bool add);
}
