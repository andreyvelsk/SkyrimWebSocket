#include "InventoryReader.h"

#include <format>
#include <unordered_map>

namespace InventoryReader
{
    // clang-format off
    static const std::unordered_map<RE::FormType, std::string> s_formTypeNames = {
        { RE::FormType::Weapon,      "Weapons"     },
        { RE::FormType::Armor,       "Apparel"     },
        { RE::FormType::Book,        "Books"       },
        { RE::FormType::AlchemyItem, "Potions"     },
        { RE::FormType::Ingredient,  "Ingredients" },
        { RE::FormType::Misc,        "Misc"        },
        { RE::FormType::Ammo,        "Ammo"        },
        { RE::FormType::KeyMaster,   "Keys"        },
        { RE::FormType::SoulGem,     "SoulGems"    },
        { RE::FormType::Scroll,      "Scrolls"     },
    };
    // clang-format on

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory();

        std::unordered_map<std::string, int32_t> categoryCounts;
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;
            auto it = s_formTypeNames.find(item->GetFormType());
            if (it != s_formTypeNames.end())
                categoryCounts[it->second] += data.first;
        }

        nlohmann::json result = nlohmann::json::array();
        for (auto& [name, count] : categoryCounts)
            result.push_back({ { "name", name }, { "count", count } });
        return result;
    }

    nlohmann::json ReadGold()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0;

        // Gold (Septim) has the well-known base-game FormID 0x0000000F.
        static constexpr RE::FormID kGoldFormID = 0x0000000F;
        auto* goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(kGoldFormID);
        if (!goldForm)
            return 0;

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormID() == kGoldFormID;
        });

        auto it = inv.find(goldForm);
        return it != inv.end() ? it->second.first : 0;
    }

    static nlohmann::json ReadItemsByType(RE::FormType formType)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([formType](RE::TESBoundObject& obj) {
            return obj.GetFormType() == formType;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            float    weight = 0.f;
            int32_t  value  = 0;
            if (const auto* wf = item->As<RE::TESWeightForm>())
                weight = wf->weight;
            if (const auto* vf = item->As<RE::TESValueForm>())
                value = vf->value;

            nlohmann::json entry;
            entry["name"]   = item->GetName();
            entry["formId"] = std::format("0x{:08X}", item->GetFormID());
            entry["count"]  = data.first;
            entry["weight"] = weight;
            entry["value"]  = value;
            result.push_back(std::move(entry));
        }
        return result;
    }

    std::function<nlohmann::json()> MakeItemsResolver(RE::FormType formType)
    {
        return [formType]() { return ReadItemsByType(formType); };
    }
}
