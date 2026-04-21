#pragma once

#include <string>

namespace GameWriter
{
    struct CommandResult
    {
        bool        success;
        std::string error;   // empty on success
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

    // Equip a known spell to a hand slot.
    // hand = "right" or "left" (default "right").
    // Must be called on the game thread.
    CommandResult EquipSpell(RE::FormID formId, const std::string& hand);

    // Unequip a known spell from a hand slot.
    // hand = "right" or "left" (default "right").
    // Must be called on the game thread.
    CommandResult UnequipSpell(RE::FormID formId, const std::string& hand);

    // Toggle favorite status on a known spell.
    // Must be called on the game thread.
    CommandResult FavoriteSpell(RE::FormID formId);

    // Mark the given quest as the HUD/compass-tracked "active" quest.
    // Clears the kActive flag on every other quest so the journal behaves as
    // if the user had selected this quest in the menu.  The target quest must
    // currently be enabled (running) — completed or unstarted quests are
    // rejected.
    // Must be called on the game thread.
    CommandResult SetActiveQuest(RE::FormID formId);
}
