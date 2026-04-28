#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace GameWriter
{
    struct CommandResult
    {
        bool           success;
        std::string    error;  // empty on success
        nlohmann::json data;   // optional result payload (null when absent)
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

    // ─── Hotkeys (Skyrim PC "1..8" quick-equip slots) ─────────────────────
    //
    // Skyrim supports 8 hotkey slots (numbered 1..8 on the PC keyboard).
    // There are two independent backing stores:
    //   • Magic    — RE::MagicFavorites::hotkeys[0..7] for spells/shouts/powers.
    //   • Items    — RE::ExtraHotkey on an inventory entry's ExtraDataList.
    // Slots are mutually exclusive: binding a new form to slot N clears any
    // previous binding on that slot (whether it was a spell or an item).

    // Bind a spell or inventory item to hotkey slot `slot` (1..8).
    // Spells must be known by the player; items must be present in the
    // inventory.  Automatically favorites the bound form (the game requires
    // this for hotkey slots to be reachable from the favorites menu).
    // Must be called on the game thread.
    CommandResult SetHotkey(std::uint8_t slot, RE::FormID formId);

    // Clear the binding on hotkey slot `slot` (1..8).  No error if the slot
    // was already empty.  Does not unfavorite the previously bound form.
    // Must be called on the game thread.
    CommandResult ClearHotkey(std::uint8_t slot);

    // Programmatically "press" hotkey slot `slot` (1..8): equips the bound
    // spell or equips/uses the bound item just as if the player pressed the
    // corresponding key in-game.  Returns an error when the slot is empty.
    // Must be called on the game thread.
    CommandResult TriggerHotkey(std::uint8_t slot);
}
