#pragma once

#include <string>
#include <unordered_map>

struct SubscriptionState
{
    int  frequencyMs  = 500;
    bool sendOnChange = false;

    // key: user-defined alias in the response JSON
    // value: registry key, e.g. "ActorValue::kHealth"
    std::unordered_map<std::string, std::string> fields;

    // previous values used for sendOnChange comparison
    std::unordered_map<std::string, float> lastValues;
};
