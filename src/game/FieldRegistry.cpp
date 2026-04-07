#include "FieldRegistry.h"

#include <nlohmann/json.hpp>

namespace FieldRegistry
{
    // clang-format off
    static const std::unordered_map<std::string, Entry> s_registry = {
        // Vitals
        { "ActorValue::kHealth",           { RE::ActorValue::kHealth,           "Current health points"             } },
        { "ActorValue::kMagicka",          { RE::ActorValue::kMagicka,          "Current magicka points"            } },
        { "ActorValue::kStamina",          { RE::ActorValue::kStamina,          "Current stamina points"            } },

        // Regeneration rates
        { "ActorValue::kHealRate",         { RE::ActorValue::kHealRate,         "Health regeneration rate"          } },
        { "ActorValue::kMagickaRate",      { RE::ActorValue::kMagickaRate,      "Magicka regeneration rate"         } },
        { "ActorValue::kStaminaRate",      { RE::ActorValue::kStaminaRate,      "Stamina regeneration rate"         } },
        { "ActorValue::kHealRateMult",     { RE::ActorValue::kHealRateMult,     "Health regeneration multiplier"    } },
        { "ActorValue::kMagickaRateMult",  { RE::ActorValue::kMagickaRateMult,  "Magicka regeneration multiplier"   } },
        { "ActorValue::kStaminaRateMult",  { RE::ActorValue::kStaminaRateMult,  "Stamina regeneration multiplier"   } },

        // Movement & inventory
        { "ActorValue::kSpeedMult",        { RE::ActorValue::kSpeedMult,        "Movement speed multiplier"         } },
        { "ActorValue::kCarryWeight",      { RE::ActorValue::kCarryWeight,      "Carry weight capacity"             } },

        // Combat
        { "ActorValue::kAttackDamageMult", { RE::ActorValue::kAttackDamageMult, "Attack damage multiplier"          } },
        { "ActorValue::kCriticalChance",   { RE::ActorValue::kCriticalChance,   "Critical hit chance"               } },

        // Resistances
        { "ActorValue::kDamageResist",     { RE::ActorValue::kDamageResist,     "Physical damage resistance"        } },
        { "ActorValue::kMagicResist",      { RE::ActorValue::kMagicResist,      "Magic resistance"                  } },
        { "ActorValue::kFireResist",       { RE::ActorValue::kFireResist,       "Fire resistance"                   } },
        { "ActorValue::kFrostResist",      { RE::ActorValue::kFrostResist,      "Frost resistance"                  } },
        { "ActorValue::kShockResist",      { RE::ActorValue::kShockResist,      "Shock resistance"                  } },
        { "ActorValue::kPoisonResist",     { RE::ActorValue::kPoisonResist,     "Poison resistance"                 } },

        // Combat skills
        { "ActorValue::kOneHanded",        { RE::ActorValue::kOneHanded,        "One-Handed skill level"            } },
        { "ActorValue::kTwoHanded",        { RE::ActorValue::kTwoHanded,        "Two-Handed skill level"            } },
        { "ActorValue::kMarksman",         { RE::ActorValue::kMarksman,         "Archery skill level"               } },
        { "ActorValue::kBlock",            { RE::ActorValue::kBlock,            "Block skill level"                 } },

        // Crafting skills
        { "ActorValue::kSmithing",         { RE::ActorValue::kSmithing,         "Smithing skill level"              } },
        { "ActorValue::kAlchemy",          { RE::ActorValue::kAlchemy,          "Alchemy skill level"               } },
        { "ActorValue::kEnchanting",       { RE::ActorValue::kEnchanting,       "Enchanting skill level"            } },

        // Armor skills
        { "ActorValue::kHeavyArmor",       { RE::ActorValue::kHeavyArmor,       "Heavy Armor skill level"           } },
        { "ActorValue::kLightArmor",       { RE::ActorValue::kLightArmor,       "Light Armor skill level"           } },

        // Stealth skills
        { "ActorValue::kPickpocket",       { RE::ActorValue::kPickpocket,       "Pickpocket skill level"            } },
        { "ActorValue::kLockpicking",      { RE::ActorValue::kLockpicking,      "Lockpicking skill level"           } },
        { "ActorValue::kSneak",            { RE::ActorValue::kSneak,            "Sneak skill level"                 } },
        { "ActorValue::kSpeech",           { RE::ActorValue::kSpeech,           "Speech skill level"                } },

        // Magic skills
        { "ActorValue::kAlteration",       { RE::ActorValue::kAlteration,       "Alteration skill level"            } },
        { "ActorValue::kConjuration",      { RE::ActorValue::kConjuration,      "Conjuration skill level"           } },
        { "ActorValue::kDestruction",      { RE::ActorValue::kDestruction,      "Destruction skill level"           } },
        { "ActorValue::kIllusion",         { RE::ActorValue::kIllusion,         "Illusion skill level"              } },
        { "ActorValue::kRestoration",      { RE::ActorValue::kRestoration,      "Restoration skill level"           } },

        // Dragon
        { "ActorValue::kDragonSouls",      { RE::ActorValue::kDragonSouls,      "Collected dragon souls"            } },
        { "ActorValue::kShoutRecoveryMult",{ RE::ActorValue::kShoutRecoveryMult,"Shout recovery multiplier"         } },
    };
    // clang-format on

    const std::unordered_map<std::string, Entry>& GetAll()
    {
        return s_registry;
    }

    std::optional<RE::ActorValue> Resolve(const std::string& key)
    {
        auto it = s_registry.find(key);
        if (it == s_registry.end())
            return std::nullopt;
        return it->second.av;
    }

    std::string BuildDescribeJson()
    {
        nlohmann::json result;
        result["type"] = "describe";
        auto& fields   = result["fields"];
        for (auto& [key, entry] : s_registry)
            fields[key] = { { "valueType", "float" }, { "description", entry.description } };
        return result.dump();
    }
}
