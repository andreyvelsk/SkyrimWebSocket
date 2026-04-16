#pragma once

#include <functional>
#include <nlohmann/json.hpp>

namespace MagicReader
{
    // Returns an array of {categoryId, name, count} for each non-empty magic category.
    // Must be called on the game thread.
    nlohmann::json ReadCategories();

    // Per-category spell readers — each must be called on the game thread.
    nlohmann::json ReadAlteration();
    nlohmann::json ReadConjuration();
    nlohmann::json ReadDestruction();
    nlohmann::json ReadIllusion();
    nlohmann::json ReadRestoration();
}
