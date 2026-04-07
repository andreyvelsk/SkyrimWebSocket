#pragma once

#include "../server/SubscriptionState.h"
#include <string>

namespace GameReader
{
    // Resolves subscription fields and builds a JSON data message.
    // Updates state.lastValues for sendOnChange tracking.
    // Returns an empty string when nothing should be sent (no changes or no fields).
    // Must be called on the game thread.
    std::string BuildSubscriptionJson(SubscriptionState& state);
}
