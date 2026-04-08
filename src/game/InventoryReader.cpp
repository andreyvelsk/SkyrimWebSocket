#include "InventoryReader.h"

#include <array>
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

    // Slot 30–43 biped object bits → human-readable names
    static const std::array<std::pair<std::uint32_t, const char*>, 14> kBipedSlotNames = { {
        { 1u << 0,  "Head"      },   // slot 30
        { 1u << 1,  "Hair"      },   // slot 31
        { 1u << 2,  "Body"      },   // slot 32
        { 1u << 3,  "Hands"     },   // slot 33
        { 1u << 4,  "Forearms"  },   // slot 34
        { 1u << 5,  "Amulet"    },   // slot 35
        { 1u << 6,  "Ring"      },   // slot 36
        { 1u << 7,  "Feet"      },   // slot 37
        { 1u << 8,  "Calves"    },   // slot 38
        { 1u << 9,  "Shield"    },   // slot 39
        { 1u << 10, "Tail"      },   // slot 40
        { 1u << 11, "LongHair"  },   // slot 41
        { 1u << 12, "Circlet"   },   // slot 42
        { 1u << 13, "Ears"      },   // slot 43
    } };
    // clang-format on

    // ─── Helpers ──────────────────────────────────────────────────────────

    // Gets the TESDescription text for forms that carry it (books, armor…).
    static std::string GetFormDescription(RE::TESBoundObject* item)
    {
        auto* desc = item->As<RE::TESDescription>();
        if (!desc)
            return "";
        RE::BSString buf;
        desc->GetDescription(buf, item);
        return buf.empty() ? "" : std::string(buf);
    }

    // Converts a SOUL_LEVEL enum value to a display string.
    static std::string SoulLevelToString(RE::SOUL_LEVEL level)
    {
        switch (level) {
            case RE::SOUL_LEVEL::kNone:    return "None";
            case RE::SOUL_LEVEL::kPetty:   return "Petty";
            case RE::SOUL_LEVEL::kLesser:  return "Lesser";
            case RE::SOUL_LEVEL::kCommon:  return "Common";
            case RE::SOUL_LEVEL::kGreater: return "Greater";
            case RE::SOUL_LEVEL::kGrand:   return "Grand";
            default:                       return "Unknown";
        }
    }

    // Builds a human-readable description for a single magic effect using its
    // magnitude and duration.  For Soul Trap the wording matches in-game tooltips.
    static std::string BuildEffectDescription(const RE::Effect* eff)
    {
        if (!eff || !eff->baseEffect)
            return "";

        const std::string   name     = eff->baseEffect->GetName();
        const float         mag      = eff->effectItem.magnitude;
        const std::uint32_t duration = eff->effectItem.duration;

        // Special-case Soul Trap so the description matches the in-game tooltip.
        if (eff->baseEffect->data.archetype == RE::EffectSetting::Archetype::kSoulTrap) {
            if (duration > 0)
                return std::format(
                    "If target dies within {} second{}, fills a soul gem",
                    duration, duration == 1u ? "" : "s");
            return "Fills a soul gem";
        }

        if (mag > 0.f && duration > 0)
            return std::format("{} {} for {} second{}",
                               name, static_cast<int>(mag),
                               duration, duration == 1u ? "" : "s");
        if (mag > 0.f)
            return std::format("{} {}", name, static_cast<int>(mag));
        if (duration > 0)
            return std::format("{} for {} second{}",
                               name, duration, duration == 1u ? "" : "s");
        return name;
    }

    // Returns a JSON object for a single magic effect with all raw parameters
    // plus a human-readable description.
    static nlohmann::json BuildEffectJson(const RE::Effect* eff)
    {
        nlohmann::json j;
        if (!eff || !eff->baseEffect) {
            j["name"]        = "";
            j["magnitude"]   = 0.f;
            j["duration"]    = 0u;
            j["description"] = "";
            return j;
        }
        j["name"]        = eff->baseEffect->GetName();
        j["magnitude"]   = eff->effectItem.magnitude;
        j["duration"]    = eff->effectItem.duration;
        j["description"] = BuildEffectDescription(eff);
        return j;
    }

    // Returns a JSON object with a combined "description" string and an "effects"
    // array for every effect on a MagicItem (potion, scroll, enchantment…).
    static nlohmann::json BuildMagicItemDetails(const RE::MagicItem* magic)
    {
        std::string    combined;
        nlohmann::json effects = nlohmann::json::array();

        if (magic) {
            for (const auto* eff : magic->effects) {
                if (!eff || !eff->baseEffect)
                    continue;
                if (!combined.empty())
                    combined += "; ";
                combined += BuildEffectDescription(eff);
                effects.push_back(BuildEffectJson(eff));
            }
        }

        nlohmann::json out;
        out["description"] = std::move(combined);
        out["effects"]     = std::move(effects);
        return out;
    }

    // Returns a JSON object describing the enchantment on an item stack, or
    // JSON null when the item carries no enchantment.
    // The object shape is: { name, description, effects[] }.
    static nlohmann::json GetEnchantmentDetails(const RE::TESBoundObject* item,
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
        if (!ench)
            return nullptr;

        auto details     = BuildMagicItemDetails(ench);
        details["name"]  = ench->GetName();
        return details;
    }

    // Returns a JSON array of body-slot strings (e.g. ["Head", "Hair"]) for
    // an armor piece, derived from its BGSBipedObjectForm slot mask.
    static nlohmann::json GetArmorBodySlots(RE::TESObjectARMO* armor)
    {
        nlohmann::json slots = nlohmann::json::array();
        if (!armor)
            return slots;

        const auto slotMask = static_cast<std::uint32_t>(armor->GetSlotMask());
        for (const auto& [bit, name] : kBipedSlotNames) {
            if (slotMask & bit)
                slots.push_back(name);
        }
        return slots;
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
                if (alch && alch->IsFood()) {
                    categoryCounts["Food"] += data.first;
                    continue;  // count as Food, not Potions
                }
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

        // kAttackDamageMult starts at 1.0 and is raised by perks (Armsman, Barbarian…).
        // Multiplying the weapon's base damage by this value gives the number shown
        // in the inventory screen.
        const float atkMult = player->AsActorValueOwner()
                                  ->GetActorValue(RE::ActorValue::kAttackDamageMult);

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j           = BuildBaseEntry(item, data);
            j["isEquipped"]  = data.second ? data.second->IsWorn() : false;

            const auto* weap = item->As<RE::TESObjectWEAP>();
            const float base = weap ? weap->GetAttackDamage() : 0.f;
            j["baseDamage"]  = base;
            j["damage"]      = base * atkMult;

            j["enchantment"] = GetEnchantmentDetails(item, data.second.get());

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

            auto* armor = item->As<RE::TESObjectARMO>();
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
                j["bodySlots"]   = GetArmorBodySlots(armor);
            } else {
                j["armorType"]   = nullptr;
                j["armorRating"] = 0.f;
                j["bodySlots"]   = nlohmann::json::array();
            }

            j["enchantment"] = GetEnchantmentDetails(item, data.second.get());

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

            auto j               = BuildBaseEntry(item, data);
            const auto* alch     = item->As<RE::AlchemyItem>();
            auto        details  = BuildMagicItemDetails(alch);
            j["description"]     = std::move(details["description"]);
            j["effects"]         = std::move(details["effects"]);
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
            auto        j        = BuildBaseEntry(item, data);
            // Scrolls are MagicItems — build the description from effect data.
            const auto* magic    = item->As<RE::MagicItem>();
            auto        details  = BuildMagicItemDetails(magic);
            j["description"]     = std::move(details["description"]);
            j["effects"]         = std::move(details["effects"]);
            result.push_back(std::move(j));
        }
        return result;
    }

    nlohmann::json ReadFood()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            if (obj.GetFormType() != RE::FormType::AlchemyItem)
                return false;
            const auto* alch = obj.As<RE::AlchemyItem>();
            return alch && alch->IsFood();
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j               = BuildBaseEntry(item, data);
            const auto* alch     = item->As<RE::AlchemyItem>();
            auto        details  = BuildMagicItemDetails(alch);
            j["description"]     = std::move(details["description"]);
            j["effects"]         = std::move(details["effects"]);
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
            auto j = BuildBaseEntry(item, data);

            const auto* gem = item->As<RE::TESSoulGem>();
            if (gem) {
                j["capacity"]      = SoulLevelToString(gem->GetMaximumCapacity());
                j["containedSoul"] = SoulLevelToString(gem->GetContainedSoul());
            } else {
                j["capacity"]      = nullptr;
                j["containedSoul"] = nullptr;
            }

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

    // ─── ReadBooks ────────────────────────────────────────────────────────

    nlohmann::json ReadBooks()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        auto inv = player->GetInventory([](RE::TESBoundObject& obj) {
            return obj.GetFormType() == RE::FormType::Book;
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

    // ─── Generic resolver (Ammo, Keys) ───────────────────────────────────

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
