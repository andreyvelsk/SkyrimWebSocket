#pragma once

#include <nlohmann/json.hpp>

namespace HotkeyReader
{
    // Returns an array of exactly 8 entries, one per hotkey slot (1..8).
    // Each entry always contains "slot" (1-based) and "bound" (bool).
    // When "bound" is true the entry also carries "kind" plus the fields
    // appropriate for that kind:
    //   kind = "spell"  → name, formId, spellType, school, cost, level, chargeTime
    //   kind = "item"   → name, formId, categoryType, count, weight, value, isFavorite
    // Must be called on the game thread.
    nlohmann::json ReadItems();
}
