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

    // Gets the TESDescription text for forms that carry it (books).
    static std::string GetFormDescription(RE::TESBoundObject* item)
    {
        auto* desc = item->As<RE::TESDescription>();
        if (!desc)
            return "";
        RE::BSString buf;
        desc->GetDescription(buf, item);
        return buf.empty() ? "" : std::string(buf);
    }

    // Converts a SOUL_LEVEL enum value to a stable string key.
    // These are internal identifiers; the game's localized name (e.g. "Petty Soul Gem")
    // is available via the soul gem item's own name field.
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

    // Looks up a GameSetting string by key (e.g. "sSkillHeavyarmor").
    // Returns an empty string when the key does not exist or is not a string setting.
    static std::string GetGMSTString(const char* key)
    {
        auto* gmst = RE::GameSettingCollection::GetSingleton();
        if (!gmst)
            return "";
        auto* setting = gmst->GetSetting(key);
        if (!setting)
            return "";
        const char* str = setting->GetString();
        return str ? str : "";
    }

    // Returns true if at least one ExtraDataList on the entry carries an ownership
    // record.  In Skyrim an item is considered stolen when it has ExtraOwnership data
    // (set when the item is picked up from a non-player owner).
    static bool IsItemStolen(const RE::InventoryEntryData* entry)
    {
        if (!entry || !entry->extraLists)
            return false;
        for (const auto* xList : *entry->extraLists) {
            if (!xList)
                continue;
            const auto* xOwner = xList->GetByType<RE::ExtraOwnership>();
            if (xOwner && xOwner->owner)
                return true;
        }
        return false;
    }

    // Returns a JSON object for a single magic effect.
    // descriptionTemplate is the localized in-game text obtained from the EffectSetting
    // via TESDescription::GetDescription.  It may contain unresolved <mag>/<dur>
    // placeholders when the engine does not substitute them for the base form context;
    // the client should substitute magnitude and duration itself.
    // Replace all occurrences of `from` with `to` inside `str`.
    static void ReplaceAll(std::string& str, const std::string_view from, const std::string& to)
    {
        for (std::size_t pos = 0; (pos = str.find(from, pos)) != std::string::npos; pos += to.size())
            str.replace(pos, from.size(), to);
    }

    // Format a float: show as integer when there is no fractional part, otherwise
    // keep one decimal place (matches vanilla inventory display convention).
    static std::string FormatMagnitude(float v)
    {
        float intpart;
        if (std::modf(v, &intpart) == 0.f)
            return std::to_string(static_cast<int>(intpart));
        return std::format("{:.1f}", v);
    }

    static nlohmann::json BuildEffectJson(const RE::Effect* eff)
    {
        nlohmann::json j;
        if (!eff || !eff->baseEffect) {
            j["name"]                = "";
            j["magnitude"]           = 0.f;
            j["duration"]            = 0u;
            j["descriptionTemplate"] = "";
            j["description"]         = "";
            return j;
        }
        j["name"]      = eff->baseEffect->GetName();
        j["magnitude"] = eff->effectItem.magnitude;
        j["duration"]  = eff->effectItem.duration;

        // EffectSetting stores its localized description in the BSFixedString
        // member magicItemDescription (DNAM subrecord).  TESDescription is not
        // in EffectSetting's inheritance chain so As<> / static_cast do not apply.
        const auto& desc = eff->baseEffect->magicItemDescription;
        std::string tmpl = desc.empty() ? "" : std::string(desc.c_str());
        j["descriptionTemplate"] = tmpl;

        // Build the ready-to-display description by substituting <mag> and <dur>.
        std::string resolved = tmpl;
        ReplaceAll(resolved, "<mag>", FormatMagnitude(eff->effectItem.magnitude));
        ReplaceAll(resolved, "<dur>", std::to_string(eff->effectItem.duration));
        j["description"] = std::move(resolved);

        return j;
    }

    // Returns a JSON array of effect objects for a MagicItem.
    static nlohmann::json BuildMagicEffectsArray(const RE::MagicItem* magic)
    {
        nlohmann::json effects = nlohmann::json::array();
        if (magic) {
            for (const auto* eff : magic->effects) {
                if (!eff || !eff->baseEffect)
                    continue;
                effects.push_back(BuildEffectJson(eff));
            }
        }
        return effects;
    }

    // Returns a JSON object describing the enchantment on an item stack, or
    // JSON null when the item carries no enchantment.
    // Shape: { "name": string, "effects": [ { name, magnitude, duration, descriptionTemplate } ] }
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

        nlohmann::json details;
        details["name"]    = ench->GetName();
        details["effects"] = BuildMagicEffectsArray(ench);
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
        j["isStolen"]   = IsItemStolen(entry);
        return j;
    }

    // Maps stable categoryId strings to the GMST key that holds the in-game
    // localized display name for that category.  Vanilla Skyrim does not expose
    // dedicated GMST strings for inventory category tab labels, so all entries
    // use an empty string and name falls back to categoryId.  The map exists to
    // make it easy to add GMST keys in future if they become available (e.g. via
    // a UI mod that registers them).
    // clang-format off
    static const std::unordered_map<std::string, const char*> s_categoryGMSTKeys = {
        { "Weapons",     "" },
        { "Apparel",     "" },
        { "Books",       "" },
        { "Potions",     "" },
        { "Food",        "" },
        { "Ingredients", "" },
        { "Misc",        "" },
        { "Ammo",        "" },
        { "Keys",        "" },
        { "SoulGems",    "" },
        { "Scrolls",     "" },
        { "Favorites",   "" },
    };
    // clang-format on

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
        for (auto& [catId, count] : categoryCounts) {
            // Attempt a GMST lookup for the localized display name.  Falls back to
            // categoryId when no GMST key is configured or the key is not found.
            std::string displayName;
            auto gmstIt = s_categoryGMSTKeys.find(catId);
            if (gmstIt != s_categoryGMSTKeys.end() && gmstIt->second[0] != '\0')
                displayName = GetGMSTString(gmstIt->second);
            if (displayName.empty())
                displayName = catId;

            result.push_back({
                { "categoryId", catId       },
                { "name",       displayName },
                { "count",      count       },
            });
        }
        if (favCount > 0)
            result.push_back({
                { "categoryId", "Favorites" },
                { "name",       "Favorites" },
                { "count",      favCount    },
            });
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

    // ─── Weapon helpers ───────────────────────────────────────────────────

    // Maps RE::WEAPON_TYPE to a stable string identifier for the API.
    static const char* WeaponTypeToString(RE::WEAPON_TYPE type)
    {
        switch (type) {
            case RE::WEAPON_TYPE::kHandToHandMelee: return "HandToHand";
            case RE::WEAPON_TYPE::kOneHandSword:    return "OneHandSword";
            case RE::WEAPON_TYPE::kOneHandDagger:   return "OneHandDagger";
            case RE::WEAPON_TYPE::kOneHandAxe:      return "OneHandAxe";
            case RE::WEAPON_TYPE::kOneHandMace:     return "OneHandMace";
            case RE::WEAPON_TYPE::kTwoHandSword:    return "TwoHandSword";
            case RE::WEAPON_TYPE::kTwoHandAxe:      return "TwoHandAxe";
            case RE::WEAPON_TYPE::kBow:             return "Bow";
            case RE::WEAPON_TYPE::kStaff:           return "Staff";
            case RE::WEAPON_TYPE::kCrossbow:        return "Crossbow";
            default:                                return "Unknown";
        }
    }

    // Returns true for weapon types that occupy both hands (two-handed melee,
    // bows, crossbows).  Staves are one-handed.
    static bool IsWeaponTwoHanded(RE::WEAPON_TYPE type)
    {
        switch (type) {
            case RE::WEAPON_TYPE::kTwoHandSword:
            case RE::WEAPON_TYPE::kTwoHandAxe:
            case RE::WEAPON_TYPE::kBow:
            case RE::WEAPON_TYPE::kCrossbow:
                return true;
            default:
                return false;
        }
    }

    // Determines which hand a weapon is currently equipped in by inspecting
    // the ExtraWorn / ExtraWornLeft flags on its extra-data lists.
    // For two-handed weapons the engine only sets kWorn (right-hand flag)
    // even though the weapon occupies both hands; isTwoHanded corrects this
    // so the caller always sees "both" when both hand slots are blocked.
    // Returns "right", "left", "both", or nullptr.
    static nlohmann::json GetWeaponEquippedHand(const RE::InventoryEntryData* entry,
                                                bool                          isTwoHanded)
    {
        if (!entry || !entry->extraLists)
            return nullptr;

        bool right = false;
        bool left  = false;
        for (const auto* xList : *entry->extraLists) {
            if (!xList)
                continue;
            if (xList->HasType(RE::ExtraDataType::kWorn))
                right = true;
            if (xList->HasType(RE::ExtraDataType::kWornLeft))
                left = true;
        }

        // Two-handed weapons always occupy both hands regardless of which
        // worn flag the engine happens to set (normally kWorn only).
        if (isTwoHanded && (right || left))
            return "both";
        if (right && left)
            return "both";
        if (right)
            return "right";
        if (left)
            return "left";
        return nullptr;
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

            // Weapon type and equip-slot metadata
            if (weap) {
                const auto wtype    = weap->GetWeaponType();
                const bool twoHand  = IsWeaponTwoHanded(wtype);
                j["weaponType"]     = WeaponTypeToString(wtype);
                j["isTwoHanded"]    = twoHand;

                // One-handed weapons (including staves) can go in either hand;
                // two-handed weapons occupy the right hand and block the left.
                nlohmann::json slots = nlohmann::json::array();
                slots.push_back("right");
                if (!twoHand)
                    slots.push_back("left");
                j["equipSlots"]     = std::move(slots);
                j["equippedHand"]   = GetWeaponEquippedHand(data.second.get(), twoHand);
            } else {
                j["weaponType"]     = nullptr;
                j["isTwoHanded"]    = false;
                j["equipSlots"]     = nlohmann::json::array({"right", "left"});
                j["equippedHand"]   = nullptr;
            }

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

        // Lookup localized armor-type display names once per call.
        // sSkillHeavyarmor / sSkillLightarmor are vanilla Skyrim GMSTs that
        // contain the localized skill (and armor-type) names, e.g.
        //   EN: "Heavy Armor" / "Light Armor"
        //   RU: "Тяжелая броня" / "Легкая броня"
        // When a GMST is not found the stable armorTypeId value is used as fallback.
        const std::string localizedHeavy   = [] {
            auto s = GetGMSTString("sSkillHeavyarmor");
            return s.empty() ? "Heavy" : s;
        }();
        const std::string localizedLight   = [] {
            auto s = GetGMSTString("sSkillLightarmor");
            return s.empty() ? "Light" : s;
        }();
        // No vanilla GMST exists for "Clothing" — keep the stable ID as display name.
        const std::string localizedClothing = "Clothing";

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;

            auto j          = BuildBaseEntry(item, data);
            j["isEquipped"] = data.second ? data.second->IsWorn() : false;

            auto* armor = item->As<RE::TESObjectARMO>();
            if (armor) {
                // armorTypeId is the stable internal key; armorType is the localized
                // display string suitable for showing directly in a UI.
                std::string armorTypeId   = "Clothing";
                std::string armorTypeName = localizedClothing;
                if (armor->HasKeywordString("ArmorHeavy")) {
                    armorTypeId   = "Heavy";
                    armorTypeName = localizedHeavy;
                } else if (armor->HasKeywordString("ArmorLight")) {
                    armorTypeId   = "Light";
                    armorTypeName = localizedLight;
                }
                j["armorTypeId"] = std::move(armorTypeId);
                j["armorType"]   = std::move(armorTypeName);

                // baseArmorRating is the raw value from the form.
                // armorRating is the effective value as shown in the inventory:
                //   baseArmorRating × (1 + kArmorPerks / 100)
                // where kArmorPerks is the bonus percentage from armor-skill perks
                // (e.g. Juggernaut for Heavy Armor, Custom Fit for Light Armor).
                const float baseArmor = armor->GetArmorRating();
                const float armorPerks = player->AsActorValueOwner()
                                             ->GetActorValue(RE::ActorValue::kArmorPerks);
                j["baseArmorRating"] = baseArmor;
                j["armorRating"]     = baseArmor * (1.0f + armorPerks / 100.0f);

                j["bodySlots"]   = GetArmorBodySlots(armor);
            } else {
                j["armorTypeId"]     = nullptr;
                j["armorType"]       = nullptr;
                j["baseArmorRating"] = 0.f;
                j["armorRating"]     = 0.f;
                j["bodySlots"]       = nlohmann::json::array();
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

            auto j           = BuildBaseEntry(item, data);
            const auto* alch = item->As<RE::AlchemyItem>();
            j["effects"]     = BuildMagicEffectsArray(alch);
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
            auto        j     = BuildBaseEntry(item, data);
            // Scrolls are MagicItems — build effects from game data (no hardcoded strings).
            const auto* magic = item->As<RE::MagicItem>();
            j["effects"]      = BuildMagicEffectsArray(magic);
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

            auto j           = BuildBaseEntry(item, data);
            const auto* alch = item->As<RE::AlchemyItem>();
            j["effects"]     = BuildMagicEffectsArray(alch);
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

    // Attempts to extract a base-damage value from various form types.
    // Different CommonLibSSE/RE versions expose damage under different
    // members/methods (GetAttackDamage, GetDamage, gamedata/data.damage, etc.).
    template <typename T>
    static float ExtractBaseDamage(const T* obj)
    {
        if (!obj)
            return 0.0f;
        if constexpr (requires(const T* x) { x->GetAttackDamage(); }) {
            return obj->GetAttackDamage();
        } else if constexpr (requires(const T* x) { x->GetDamage(); }) {
            return obj->GetDamage();
        } else if constexpr (requires(const T* x) { x->gamedata.damage; }) {
            return obj->gamedata.damage;
        } else if constexpr (requires(const T* x) { x->data.damage; }) {
            return obj->data.damage;
        } else if constexpr (requires(const T* x) { x->damage; }) {
            return obj->damage;
        } else {
            return 0.0f;
        }
    }

    // ─── Generic resolver (Ammo, Keys) ───────────────────────────────────

    static nlohmann::json ReadItemsByType(RE::FormType formType)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return nlohmann::json::array();

        // Compute attack multiplier once (matches ReadWeapons behaviour).
        const float atkMult = player->AsActorValueOwner()
                                  ->GetActorValue(RE::ActorValue::kAttackDamageMult);

        auto inv = player->GetInventory([formType](RE::TESBoundObject& obj) {
            return obj.GetFormType() == formType;
        });

        nlohmann::json result = nlohmann::json::array();
        for (auto& [item, data] : inv) {
            if (!item || data.first <= 0)
                continue;
            auto j = BuildBaseEntry(item, data);
            j["isEquipped"] = data.second ? data.second->IsWorn() : false;

            // For Ammo entries include an effective damage field.
            if (formType == RE::FormType::Ammo) {
                float base = 0.0f;
                if (const auto* ammo = item->As<RE::TESAmmo>())
                    base = ammo->GetDamage();
                else if (const auto* weap = item->As<RE::TESObjectWEAP>())
                    base = weap->GetAttackDamage();
                j["baseDamage"] = base;
                j["damage"] = base * atkMult;
            }

            result.push_back(std::move(j));
        }
        return result;
    }

    std::function<nlohmann::json()> MakeItemsResolver(RE::FormType formType)
    {
        return [formType]() { return ReadItemsByType(formType); };
    }
}
