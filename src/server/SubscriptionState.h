#pragma once

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

struct SubscriptionState
{
    int  frequencyMs  = 500;
    bool sendOnChange = false;

    // key: user-defined alias in the response JSON
    // value: registry key, e.g. "ActorValue::kHealth" or "Inventory::kWeapon"
    std::unordered_map<std::string, std::string> fields;

    // previous values used for sendOnChange comparison
    // For actor values (float types)
    std::unordered_map<std::string, float> lastValues;
    
    // previous JSON values used for sendOnChange comparison
    // For inventory and other complex types
    std::unordered_map<std::string, nlohmann::json> lastJsonValues;
};
