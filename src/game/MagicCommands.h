#pragma once

#include "Common.h"

#include <string>

namespace MagicCommands
{
    using Common::CommandResult;

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

    // Equip a known dragon shout to the voice slot.
    // Must be called on the game thread.
    CommandResult EquipShout(RE::FormID formId);

    // Remove a dragon shout from the voice slot.
    // Must be called on the game thread.
    CommandResult UnequipShout(RE::FormID formId);

    // Equip a known power or lesser power to the voice slot.
    // Must be called on the game thread.
    CommandResult EquipPower(RE::FormID formId);

    // Toggle favorite status on a known dragon shout.
    // Must be called on the game thread.
    CommandResult FavoriteShout(RE::FormID formId);
}
