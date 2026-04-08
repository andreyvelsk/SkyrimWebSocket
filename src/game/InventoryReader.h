#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace InventoryReader
{
    // Returns an array of {name, count} for each non-empty inventory category.
    // Excludes food from Potions and gold from Misc. Includes a Favorites count.
    // Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Returns the player's current gold amount.
    // Must be called on the game thread.
    nlohmann::json ReadGold();

    // Per-category item readers — each must be called on the game thread.
    nlohmann::json ReadWeapons();
    nlohmann::json ReadApparel();
    nlohmann::json ReadPotions();
    nlohmann::json ReadIngredients();
    nlohmann::json ReadMisc();
    nlohmann::json ReadScrolls();
    nlohmann::json ReadSoulGems();
    nlohmann::json ReadFavorites();

    // Generic resolver for categories without specialised data (Books, Ammo, Keys).
    // The returned function must be called on the game thread.
    std::function<nlohmann::json()> MakeItemsResolver(RE::FormType formType);
}
