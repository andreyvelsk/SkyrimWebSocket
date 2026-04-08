#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace InventoryReader
{
    // Returns an array of {name, count} for each non-empty inventory category.
    // Excludes gold from Misc. Food items are counted under "Food", not "Potions".
    // Includes a Favorites count when any item is favourited.
    // Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Returns the player's current gold amount.
    // Must be called on the game thread.
    nlohmann::json ReadGold();

    // Per-category item readers — each must be called on the game thread.
    nlohmann::json ReadWeapons();
    nlohmann::json ReadApparel();
    nlohmann::json ReadPotions();
    nlohmann::json ReadFood();
    nlohmann::json ReadIngredients();
    nlohmann::json ReadMisc();
    nlohmann::json ReadBooks();
    nlohmann::json ReadScrolls();
    nlohmann::json ReadSoulGems();
    nlohmann::json ReadFavorites();

    // Generic resolver for categories without specialised data (Ammo, Keys).
    // The returned function must be called on the game thread.
    std::function<nlohmann::json()> MakeItemsResolver(RE::FormType formType);
}
