#pragma once

#include "Common.h"

#include <cstdint>

namespace HotkeyCommands
{
    using Common::CommandResult;

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
