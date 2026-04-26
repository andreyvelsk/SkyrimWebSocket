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

        const bool inGame = FieldRegistry::IsInGame();

        auto* player = RE::PlayerCharacter::GetSingleton();
        // When not in-game we still need to publish nulls for in-game fields,
        // so don't bail out on a missing actor-value owner here.
        auto* avo    = player ? player->AsActorValueOwner() : nullptr;

        nlohmann::json dataFields;
        bool           anyChanged = false;

        for (auto& [alias, registryKey] : state.fields) {
            // --- float ActorValue fields ---
            auto entryOpt = FieldRegistry::Resolve(registryKey);
            if (entryOpt) {
                const auto& entry = entryOpt.value();

                // Out-of-game: surface explicit null instead of a stale 0.
                if (entry.requiresInGame && (!inGame || !avo)) {
                    std::string valStr = "null";
                    if (state.sendOnChange) {
                        auto it = state.lastValues.find(alias);
                        if (it != state.lastValues.end() && it->second == valStr)
                            continue;
                        anyChanged = true;
                    }
                    dataFields[alias]       = nullptr;
                    state.lastValues[alias] = std::move(valStr);
                    continue;
                }

                float val = 0.f;

                switch (entry.valueType) {
                    case FieldRegistry::ValueType::kCurrent:
                        val = avo->GetActorValue(entry.av);
                        break;
                    case FieldRegistry::ValueType::kPermanent:
                        val = avo->GetPermanentActorValue(entry.av);
                        break;
                    case FieldRegistry::ValueType::kBase:
                        val = avo->GetBaseActorValue(entry.av);
                        break;
                    case FieldRegistry::ValueType::kClamped:
                        val = avo->GetClampedActorValue(entry.av);
                        break;
                }

                if (!std::isfinite(val))
                    val = 0.f;

                std::string valStr = nlohmann::json(val).dump();
                if (state.sendOnChange) {
                    auto it = state.lastValues.find(alias);
                    if (it != state.lastValues.end() && it->second == valStr)
                        continue;  // value unchanged — skip
                    anyChanged = true;
                }

                dataFields[alias]       = val;
                state.lastValues[alias] = std::move(valStr);
                continue;
            }

            // --- JSON fields (inventory, etc.) ---
            auto jsonEntryOpt = FieldRegistry::ResolveJson(registryKey);
            if (jsonEntryOpt) {
                nlohmann::json val;
                if (jsonEntryOpt->requiresInGame && !inGame) {
                    val = nullptr;
                } else {
                    val = jsonEntryOpt->resolve();
                }
                std::string valStr = val.dump();

                if (state.sendOnChange) {
                    auto it = state.lastValues.find(alias);
                    if (it != state.lastValues.end() && it->second == valStr)
                        continue;  // value unchanged — skip
                    anyChanged = true;
                }

                dataFields[alias]       = std::move(val);
                state.lastValues[alias] = std::move(valStr);
            }
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
        msg["id"]     = state.id;
        msg["ts"]     = nowMs;
        msg["fields"] = dataFields;
        return msg.dump();
    }
}
