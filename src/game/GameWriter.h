#pragma once

#include <string>

namespace GameWriter
{
    struct CommandResult
    {
        bool        success;
        std::string error;     // empty on success
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

    // Equip a spell to a hand or both.
    // hand = "right", "left", or "both".
    // Must be called on the game thread.
    CommandResult EquipSpell(RE::FormID formId, const std::string& hand);

    // Unequip a spell from a hand.
    // hand = "right", "left", or "both".
    // Must be called on the game thread.
    CommandResult UnequipSpell(RE::FormID formId, const std::string& hand);

    // Toggle favorite on a spell (if supported). Must be called on the game
    // thread. Returns an error when the operation is not available.
    CommandResult FavoriteSpell(RE::FormID formId);
}
