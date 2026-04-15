#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace Magic
{
    // Returns an array of {categoryId, name, count} for each non-empty magic
    // category (e.g. "Destruction").  Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Returns all known magic items (spells) for the player as a JSON array.
    // Must be called on the game thread.
    nlohmann::json ReadAllSpells();

    // Returns favorited spells (if detectable) as a JSON array. Must be called
    // on the game thread.
    nlohmann::json ReadFavorites();

    // Generic resolver for magic category lists (by categoryId). The returned
    // function must be called on the game thread.
    std::function<nlohmann::json()> MakeCategoryResolver(const std::string& categoryId);
}
