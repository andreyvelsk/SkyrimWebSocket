#include "GameReader.h"
#include "FieldRegistry.h"

#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>

namespace GameReader
{
    std::string BuildSubscriptionJson(SubscriptionState& state)
    {
        if (state.fields.empty())
            return {};

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return {};

        auto* avo = player->AsActorValueOwner();

        nlohmann::json dataFields;
        bool           anyChanged = false;

        for (auto& [alias, registryKey] : state.fields) {
            auto avOpt = FieldRegistry::Resolve(registryKey);
            if (!avOpt)
                continue;

            float val = avo->GetActorValue(*avOpt);
            if (!std::isfinite(val))
                val = 0.f;

            if (state.sendOnChange) {
                auto it = state.lastValues.find(alias);
                if (it != state.lastValues.end() && it->second == val)
                    continue;  // value unchanged — skip
                anyChanged = true;
            }

            dataFields[alias]       = val;
            state.lastValues[alias] = val;
        }

        if (state.sendOnChange && !anyChanged)
            return {};

        if (dataFields.empty())
            return {};

        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

        nlohmann::json msg;
        msg["type"]   = "data";
        msg["ts"]     = nowMs;
        msg["fields"] = dataFields;
        return msg.dump();
    }
}
