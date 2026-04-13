#pragma once

#include <string>
#include <vector>

namespace GameWriter
{
    struct CommandResult
    {
        bool                     success;
        std::string              error;     // empty on success
        std::vector<std::string> debugLog;  // debug messages sent back via WebSocket
    };

    // Equip an item.
    // For weapons: hand = "right" or "left" (default "right").
    // For apparel/shields/ammo: hand is ignored.
    // Must be called on the game thread.
    CommandResult EquipItem(RE::FormID formId, const std::string& hand);

    // Unequip an item.
    // For weapons: hand = "right" or "left" to specify which hand.
    // For apparel/shields/ammo: hand is ignored.
    // Must be called on the game thread.
    CommandResult UnequipItem(RE::FormID formId, const std::string& hand);

    // Use (consume) a consumable item: potions, food, ingredients, scrolls.
    // Must be called on the game thread.
    CommandResult UseItem(RE::FormID formId);

    // Drop item(s) from inventory.
    // Must be called on the game thread.
    CommandResult DropItem(RE::FormID formId, int count);

    // Toggle favorite status on an item.
    // Must be called on the game thread.
    CommandResult FavoriteItem(RE::FormID formId);
}
