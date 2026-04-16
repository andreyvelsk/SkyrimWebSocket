#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace MagicReader
{
    // Returns an array of {categoryId, name, count} for each non-empty magic school
    // that the player knows spells in. Includes player spells, powers, and shouts.
    // Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Per-category spell readers — each must be called on the game thread.
    // Each returns an array of spell objects with the fields:
    //   - name (string)
    //   - formId (hex string)
    //   - categoryId (string) — magic school identifier
    //   - categoryName (string) — localized magic school name
    //   - cost (integer) — magicka cost
    //   - level (string) — required level: "Novice", "Apprentice", "Adept", "Expert", "Master"
    //   - isEquipped (bool) — currently equipped in hand? (Player spells only)
    //   - equippedHand (string or null) — "right", "left", or null
    //   - isTwoHanded (bool) — always true for player spells (occupies both hands)
    //   - effects (array) — same format as Inventory item effects

    // Player spells (most common - obtained through training, loot, level up)
    nlohmann::json ReadPlayerSpells();

    // Lesser powers (typically 1 use per day)
    nlohmann::json ReadLesserPowers();

    // Powers and racial power (passive abilities, racial powers)
    nlohmann::json ReadPowers();

    // Shouts (mostly from main questline / dragon souls)
    nlohmann::json ReadShouts();

    // Diseases & poisons (passive effects but technically SpellItems)
    nlohmann::json ReadDiseases();

    // Helper to build a custom spell resolver for a specific spell type/condition.
    // The returned function must be called on the game thread.
    std::function<nlohmann::json()> MakeSpellResolver(
        RE::ActorValue school,
        std::function<bool(RE::SpellItem*)> predicate);
}
