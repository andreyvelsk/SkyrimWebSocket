#include "GameReader.h"
#include "FieldRegistry.h"
#include "InventoryRegistry.h"

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
            // Check if this is an Inventory request
            if (registryKey.find("Inventory::") == 0) {
                std::string invType = registryKey.substr(11);  // Remove "Inventory::" prefix
                nlohmann::json invData;

                if (invType == "kAll") {
                    invData = InventoryRegistry::GetInventoryJson(player);
                } else if (invType == "kWeapon") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kWeapon, player);
                } else if (invType == "kArmor") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kArmor, player);
                } else if (invType == "kAmmo") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kAmmo, player);
                } else if (invType == "kPotion") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kPotion, player);
                } else if (invType == "kFood") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kFood, player);
                } else if (invType == "kBook") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kBook, player);
                } else if (invType == "kIngredient") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kIngredient, player);
                } else if (invType == "kMisc") {
                    invData = InventoryRegistry::GetInventoryByTypeJson(InventoryRegistry::ItemType::kMisc, player);
                } else {
                    continue;
                }

                if (state.sendOnChange) {
                    auto it = state.lastJsonValues.find(alias);
                    if (it != state.lastJsonValues.end() && it->second == invData)
                        continue;  // value unchanged — skip
                    anyChanged = true;
                }

                dataFields[alias]                  = invData;
                state.lastJsonValues[alias] = invData;
                continue;
            }

            // Regular ActorValue field
            auto entryOpt = FieldRegistry::Resolve(registryKey);
            if (!entryOpt)
                continue;

            const auto& entry = entryOpt.value();
            float val         = 0.f;

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
