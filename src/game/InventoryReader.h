#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace InventoryReader
{
    // Returns an array of {name, count} for each non-empty inventory category.
    // Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Returns the player's current gold amount.
    // Must be called on the game thread.
    nlohmann::json ReadGold();

    // Returns a factory closure that reads all items of the given FormType.
    // The returned function must be called on the game thread.
    std::function<nlohmann::json()> MakeItemsResolver(RE::FormType formType);
}
