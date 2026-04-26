#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

namespace FieldRegistry
{
    enum class ValueType
    {
        kCurrent,    // GetActorValue - current value with temporary modifiers
        kPermanent,  // GetPermanentActorValue - base permanent value
        kBase,       // GetBaseActorValue - explicit base value (similar to permanent)
        kClamped     // GetClampedActorValue - value clamped to valid min/max ranges
    };

    struct Entry
    {
        RE::ActorValue av;
        std::string    description;
        ValueType      valueType      = ValueType::kCurrent;
        // If true, the resolver requires a loaded save (player in a cell).
        // When the game is sitting on the main menu / between saves, the
        // dispatcher will emit `null` for this field instead of reading it.
        bool           requiresInGame = true;
    };

    // A registry entry whose value is produced by an arbitrary resolver
    // that returns a JSON value (array, object, or scalar).
    struct JsonEntry
    {
        std::string                    description;
        std::string                    valueTypeName;  // e.g. "array", "object"
        std::function<nlohmann::json()> resolve;
        // See Entry::requiresInGame.
        bool                           requiresInGame = true;
    };

    const std::unordered_map<std::string, Entry>& GetAll();
    std::optional<Entry>                           Resolve(const std::string& key);
    std::optional<JsonEntry>                       ResolveJson(const std::string& key);

    // Returns true once a save is loaded and the player is present in a cell.
    // While the main menu is open or during a load screen, returns false.
    // Cheap to call from the game thread.
    bool IsInGame();
}
