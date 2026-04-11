#pragma once

#include <string>
#include <unordered_map>

struct SubscriptionState
{
    std::string id;

    int  frequencyMs  = 500;
    bool sendOnChange = false;

    // key: user-defined alias in the response JSON
    // value: registry key, e.g. "ActorValue::kHealth"
    std::unordered_map<std::string, std::string> fields;

    // previous serialised values used for sendOnChange comparison.
    // stored as JSON dump strings so float and JSON fields can be compared uniformly.
    std::unordered_map<std::string, std::string> lastValues;
};
