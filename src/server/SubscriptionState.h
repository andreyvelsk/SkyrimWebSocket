#pragma once

#include <cstdint>
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

    // For event-driven fields (see EventBus): the EventBus version observed
    // the last time the alias was resolved.  When the current EventBus
    // version still matches and the alias already has an entry in lastValues,
    // the resolver is skipped entirely on this tick.
    std::unordered_map<std::string, std::uint64_t> lastVersions;
};
