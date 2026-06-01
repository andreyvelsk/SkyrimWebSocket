#pragma once

#include <nlohmann/json.hpp>

namespace MagicReader
{
    // Returns an array of {categoryId, name, count} for each non-empty magic school
    // known by the player.  Only regular castable spells (SpellType::kSpell) are counted.
    // Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Per-school spell readers — each must be called on the game thread.
    nlohmann::json ReadDestruction();
    nlohmann::json ReadAlteration();
    nlohmann::json ReadConjuration();
    nlohmann::json ReadIllusion();
    nlohmann::json ReadRestoration();
    nlohmann::json ReadEnchanting();

    // Dragon shout reader — must be called on the game thread.
    nlohmann::json ReadShouts();

    // Power readers (greater and lesser) — each must be called on the game thread.
    nlohmann::json ReadPowers();
    nlohmann::json ReadLesserPowers();
}
