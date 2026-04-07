#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace FieldRegistry
{
    struct Entry
    {
        RE::ActorValue av;
        std::string    description;
    };

    const std::unordered_map<std::string, Entry>& GetAll();
    std::optional<RE::ActorValue>                  Resolve(const std::string& key);
    std::string                                    BuildDescribeJson();
}
