#pragma once

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
        ValueType      valueType = ValueType::kCurrent;
    };

    const std::unordered_map<std::string, Entry>& GetAll();
    std::optional<Entry>                           Resolve(const std::string& key);
    std::string                                    BuildDescribeJson();
}
