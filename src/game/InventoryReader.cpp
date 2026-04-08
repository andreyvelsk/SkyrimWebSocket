#include "InventoryReader.h"

#include <format>
#include <unordered_map>

namespace InventoryReader
{
    static constexpr RE::FormID kGoldFormID = 0x0000000F;

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

    // ─── Helpers ──────────────────────────────────────────────────────────

    // Gets the TESDescription text for forms that carry it (armor, soul gems, scrolls…).
    static std::string GetFormDescription(RE::TESBoundObject* item)
    {
        const auto* desc = item->As<RE::TESDescription>();
        if (!desc)
            return "";
        RE::BSString buf;
        desc->GetDescription(buf, item);
        return buf.empty() ? "" : std::string(buf);
    }

    // Builds a human-readable summary from a MagicItem's effect list (used for potions).
    static std::string GetMagicItemEffectsSummary(const RE::MagicItem* magic)
    {
        if (!magic)
            return "";
        std::string result;
        for (const auto* eff : magic->effects) {
            if (!eff || !eff->baseEffect)
                continue;
            if (!result.empty())
                result += "; ";
            result += eff->baseEffect->GetName();
        }
        return result;
    }

    // Returns the name of the enchantment on this item stack (custom first, then base-form).
    static std::string GetEnchantmentName(const RE::TESBoundObject* item,
                                          const RE::InventoryEntryData* entry)
    {
        RE::EnchantmentItem* ench = nullptr;
        if (entry && entry->extraLists) {
            for (const auto* xList : *entry->extraLists) {
                if (!xList)
                    continue;
                const auto* xEnch = xList->GetByType<RE::ExtraEnchantment>();
                if (xEnch && xEnch->enchantment) {
                    ench = xEnch->enchantment;
                    break;
                }
            }
        }
        if (!ench) {
            if (const auto* ef = item->As<RE::TESEnchantableForm>())
                ench = ef->formEnchanting;
        }
        return ench ? ench->GetName() : "";
    }

    // Builds the fields common to every inventory item.
    static nlohmann::json BuildBaseEntry(
        RE::TESBoundObject*                                               item,
        const std::pair<int, std::unique_ptr<RE::InventoryEntryData>>& data)
    {
        auto* entry = data.second.get();
        nlohmann::json j;
        j["name"]       = item->GetName();
        j["formId"]     = std::format("0x{:08X}", item->GetFormID());
        j["count"]      = data.first;
        j["weight"]     = entry ? entry->GetWeight() : 0.f;
        j["value"]      = entry ? entry->GetValue() : 0;
        j["isFavorite"] = entry ? entry->IsFavorited() : false;
        return j;
    }

    // ─── ReadCategories ───────────────────────────────────────────────────

    nlohmann::json ReadCategories()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory();

        std::unordered_map<std::string, int32_t> categoryCounts;
        int32_t                                  favCount = 0;

        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            if (data.second && data.second->IsFavorited())
                ++favCount;

            if (item->GetFormID() == kGoldFormID)
                continue;

            if (item->GetFormType() == RE::FormType::AlchemyItem) {
                const auto* alch = item->As<RE::AlchemyItem>();
                if (alch && alch->IsFood())
                    continue;
            }

            auto it = s_formTypeNames.find(item->GetFormType());
            if (it != s_formTypeNames.end())
                categoryCounts[it->second] += data.first;
        }

        nlohmann::json result = nlohmann::json::array();
        for (auto& [name, count] : categoryCounts)
            result.push_back({ { "name", name }, { "count", count } });
        if (favCount > 0)
            result.push_back({ { "name", "Favorites" }, { "count", favCount } });
        return result;
    }

    // ─── ReadGold ─────────────────────────────────────────────────────────

    nlohmann::json ReadGold()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return 0;

        auto* goldForm = RE::TESForm::LookupByID<RE::TESBoundObject>(kGoldFormID);
        if (!goldForm)
            return 0;

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormID() == kGoldFormID;
        });

        auto it = inv.find(goldForm);
        return it != inv.end() ? it->second.first : 0;
    }

    // ─── Per-type item readers ────────────────────────────────────────────

    nlohmann::json ReadWeapons()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::Weapon;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j           = BuildBaseEntry(item, data);
            j["isEquipped"]  = data.second ? data.second->IsWorn() : false;

            const auto* weap = item->As<RE::TESObjectWEAP>();
            j["damage"]      = weap ? weap->GetAttackDamage() : 0.f;

            j["enchantment"] = GetEnchantmentName(item, data.second.get());

            auto charge                = data.second ? data.second->GetEnchantmentCharge()
                                                     : std::optional<double>{};
            j["enchantmentCharge"]     = charge.has_value()
                                             ? nlohmann::json(charge.value())
                                             : nlohmann::json(nullptr);

            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadApparel()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::Armor;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j          = BuildBaseEntry(item, data);
            j["isEquipped"] = data.second ? data.second->IsWorn() : false;

            const auto* armor = item->As<RE::TESObjectARMO>();
            if (armor) {
                // Determine armor type via keywords (BGSBipedObjectForm::GetArmorType is
                // not exposed in CommonLibSSE-NG; keyword checking is the safe approach).
                std::string armorType = "Clothing";
                if (armor->HasKeywordString("ArmorHeavy"))
                    armorType = "Heavy";
                else if (armor->HasKeywordString("ArmorLight"))
                    armorType = "Light";
                j["armorType"]   = std::move(armorType);
                j["armorRating"] = armor->GetArmorRating();
            } else {
                j["armorType"]   = nullptr;
                j["armorRating"] = 0.f;
            }

            j["enchantment"] = GetEnchantmentName(item, data.second.get());

            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadPotions()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            if (obj.GetFormType() != RE::FormType::AlchemyItem)
                return false;
            const auto* alch = obj.As<RE::AlchemyItem>();
            return alch && !alch->IsFood();
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j           = BuildBaseEntry(item, data);
            const auto* alch = item->As<RE::AlchemyItem>();
            j["description"] = alch ? GetMagicItemEffectsSummary(alch) : "";
            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadIngredients()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::Ingredient;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j = BuildBaseEntry(item, data);

            const auto*    ingr    = item->As<RE::IngredientItem>();
            nlohmann::json effects = nlohmann::json::array();
            if (ingr) {
                for (std::uint32_t i = 0; i < ingr->effects.size() && i < 4; ++i) {
                    const auto* eff = ingr->effects[i];
                    if (!eff || !eff->baseEffect)
                        continue;
                    const bool known =
                        (ingr->gamedata.knownEffectFlags & (static_cast<std::uint16_t>(1) << i)) != 0;
                    effects.push_back({
                        { "name",  eff->baseEffect->GetName() },
                        { "known", known                      },
                    });
                }
            }
            j["effects"] = std::move(effects);

            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadMisc()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::Misc &&
                   obj.GetFormID() != kGoldFormID;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;
            result.push_back(BuildBaseEntry(item, data));
        }
        return result;
    }

    nlohmann::json ReadScrolls()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::Scroll;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;
            auto j           = BuildBaseEntry(item, data);
            j["description"] = GetFormDescription(item);
            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadSoulGems()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::SoulGem;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;
            auto j           = BuildBaseEntry(item, data);
            j["description"] = GetFormDescription(item);
            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadFavorites()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory();

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;
            if (!data.second || !data.second->IsFavorited())
                continue;

            auto j          = BuildBaseEntry(item, data);
            j["isEquipped"] = data.second->IsWorn();

            auto it   = s_formTypeNames.find(item->GetFormType());
            j["type"] = (it != s_formTypeNames.end()) ? it->second : "Unknown";

            result.push_back(std::move(j));
        }
        return result;
    }

    // ─── Generic resolver (Books, Ammo, Keys) ────────────────────────────

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
            result.push_back(BuildBaseEntry(item, data));
        }
        return result;
    }

    std::function<nlohmann::json()> MakeItemsResolver(RE::FormType formType)
    {
        return [formType]() { return ReadItemsByType(formType); };
    }
}
