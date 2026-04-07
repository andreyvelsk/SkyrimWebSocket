#include "FieldRegistry.h"

#include <nlohmann/json.hpp>

namespace FieldRegistry
{
    // clang-format off
    static const std::unordered_map<std::string, Entry> s_registry = {
        // Vitals (Current)
        { "ActorValue::kHealth",           { RE::ActorValue::kHealth,           "Current health points",                    ValueType::kCurrent   } },
        { "ActorValue::kMagicka",          { RE::ActorValue::kMagicka,          "Current magicka points",                   ValueType::kCurrent   } },
        { "ActorValue::kStamina",          { RE::ActorValue::kStamina,          "Current stamina points",                   ValueType::kCurrent   } },

        // Vitals (Permanent)
        { "ActorValue::kHealth::Permanent",    { RE::ActorValue::kHealth,           "Permanent health points base value",       ValueType::kPermanent } },
        { "ActorValue::kMagicka::Permanent",   { RE::ActorValue::kMagicka,          "Permanent magicka points base value",      ValueType::kPermanent } },
        { "ActorValue::kStamina::Permanent",   { RE::ActorValue::kStamina,          "Permanent stamina points base value",      ValueType::kPermanent } },

        // Regeneration rates
        { "ActorValue::kHealRate",         { RE::ActorValue::kHealRate,         "Health regeneration rate",                 ValueType::kCurrent   } },
        { "ActorValue::kMagickaRate",      { RE::ActorValue::kMagickaRate,      "Magicka regeneration rate",                ValueType::kCurrent   } },
        { "ActorValue::kStaminaRate",      { RE::ActorValue::kStaminaRate,      "Stamina regeneration rate",                ValueType::kCurrent   } },
        { "ActorValue::kHealRateMult",     { RE::ActorValue::kHealRateMult,     "Health regeneration multiplier",           ValueType::kCurrent   } },
        { "ActorValue::kMagickaRateMult",  { RE::ActorValue::kMagickaRateMult,  "Magicka regeneration multiplier",          ValueType::kCurrent   } },
        { "ActorValue::kStaminaRateMult",  { RE::ActorValue::kStaminaRateMult,  "Stamina regeneration multiplier",          ValueType::kCurrent   } },

        // Regeneration rates (Permanent)
        { "ActorValue::kHealRate::Permanent",      { RE::ActorValue::kHealRate,         "Permanent health regeneration rate",           ValueType::kPermanent } },
        { "ActorValue::kMagickaRate::Permanent",   { RE::ActorValue::kMagickaRate,      "Permanent magicka regeneration rate",          ValueType::kPermanent } },
        { "ActorValue::kStaminaRate::Permanent",   { RE::ActorValue::kStaminaRate,      "Permanent stamina regeneration rate",          ValueType::kPermanent } },
        { "ActorValue::kHealRateMult::Permanent",  { RE::ActorValue::kHealRateMult,     "Permanent health regeneration multiplier",     ValueType::kPermanent } },
        { "ActorValue::kMagickaRateMult::Permanent", { RE::ActorValue::kMagickaRateMult, "Permanent magicka regeneration multiplier",    ValueType::kPermanent } },
        { "ActorValue::kStaminaRateMult::Permanent", { RE::ActorValue::kStaminaRateMult, "Permanent stamina regeneration multiplier",    ValueType::kPermanent } },

        // Movement & inventory
        { "ActorValue::kSpeedMult",        { RE::ActorValue::kSpeedMult,        "Movement speed multiplier",                ValueType::kCurrent   } },
        { "ActorValue::kCarryWeight",      { RE::ActorValue::kCarryWeight,      "Carry weight capacity",                     ValueType::kCurrent   } },

        // Movement & inventory (Permanent)
        { "ActorValue::kSpeedMult::Permanent",   { RE::ActorValue::kSpeedMult,        "Permanent movement speed multiplier",      ValueType::kPermanent } },
        { "ActorValue::kCarryWeight::Permanent", { RE::ActorValue::kCarryWeight,      "Permanent carry weight capacity",          ValueType::kPermanent } },

        // Combat
        { "ActorValue::kAttackDamageMult", { RE::ActorValue::kAttackDamageMult, "Attack damage multiplier",                 ValueType::kCurrent   } },
        { "ActorValue::kCriticalChance",   { RE::ActorValue::kCriticalChance,   "Critical hit chance",                      ValueType::kCurrent   } },

        // Combat (Permanent)
        { "ActorValue::kAttackDamageMult::Permanent", { RE::ActorValue::kAttackDamageMult, "Permanent attack damage multiplier",     ValueType::kPermanent } },
        { "ActorValue::kCriticalChance::Permanent",   { RE::ActorValue::kCriticalChance,   "Permanent critical hit chance",         ValueType::kPermanent } },

        // Resistances
        { "ActorValue::kDamageResist",     { RE::ActorValue::kDamageResist,     "Physical damage resistance",               ValueType::kCurrent   } },
        { "ActorValue::kResistMagic",      { RE::ActorValue::kResistMagic,      "Magic resistance",                         ValueType::kCurrent   } },
        { "ActorValue::kResistFire",       { RE::ActorValue::kResistFire,       "Fire resistance",                          ValueType::kCurrent   } },
        { "ActorValue::kResistFrost",      { RE::ActorValue::kResistFrost,      "Frost resistance",                         ValueType::kCurrent   } },
        { "ActorValue::kResistShock",      { RE::ActorValue::kResistShock,      "Shock resistance",                         ValueType::kCurrent   } },
        { "ActorValue::kPoisonResist",     { RE::ActorValue::kPoisonResist,     "Poison resistance",                        ValueType::kCurrent   } },

        // Resistances (Permanent)
        { "ActorValue::kDamageResist::Permanent",  { RE::ActorValue::kDamageResist,     "Permanent physical damage resistance",     ValueType::kPermanent } },
        { "ActorValue::kResistMagic::Permanent",   { RE::ActorValue::kResistMagic,      "Permanent magic resistance",              ValueType::kPermanent } },
        { "ActorValue::kResistFire::Permanent",    { RE::ActorValue::kResistFire,       "Permanent fire resistance",               ValueType::kPermanent } },
        { "ActorValue::kResistFrost::Permanent",   { RE::ActorValue::kResistFrost,      "Permanent frost resistance",              ValueType::kPermanent } },
        { "ActorValue::kResistShock::Permanent",   { RE::ActorValue::kResistShock,      "Permanent shock resistance",              ValueType::kPermanent } },
        { "ActorValue::kPoisonResist::Permanent",  { RE::ActorValue::kPoisonResist,     "Permanent poison resistance",             ValueType::kPermanent } },

        // Combat skills
        { "ActorValue::kOneHanded",        { RE::ActorValue::kOneHanded,        "One-Handed skill level",                   ValueType::kCurrent   } },
        { "ActorValue::kTwoHanded",        { RE::ActorValue::kTwoHanded,        "Two-Handed skill level",                   ValueType::kCurrent   } },
        { "ActorValue::kArchery",          { RE::ActorValue::kArchery,          "Archery skill level",                      ValueType::kCurrent   } },
        { "ActorValue::kBlock",            { RE::ActorValue::kBlock,            "Block skill level",                        ValueType::kCurrent   } },

        // Combat skills (Permanent)
        { "ActorValue::kOneHanded::Permanent",  { RE::ActorValue::kOneHanded,        "One-Handed skill level base value",       ValueType::kPermanent } },
        { "ActorValue::kTwoHanded::Permanent",  { RE::ActorValue::kTwoHanded,        "Two-Handed skill level base value",       ValueType::kPermanent } },
        { "ActorValue::kArchery::Permanent",    { RE::ActorValue::kArchery,          "Archery skill level base value",          ValueType::kPermanent } },
        { "ActorValue::kBlock::Permanent",      { RE::ActorValue::kBlock,            "Block skill level base value",            ValueType::kPermanent } },

        // Crafting skills
        { "ActorValue::kSmithing",         { RE::ActorValue::kSmithing,         "Smithing skill level",                     ValueType::kCurrent   } },
        { "ActorValue::kAlchemy",          { RE::ActorValue::kAlchemy,          "Alchemy skill level",                      ValueType::kCurrent   } },
        { "ActorValue::kEnchanting",       { RE::ActorValue::kEnchanting,       "Enchanting skill level",                   ValueType::kCurrent   } },

        // Crafting skills (Permanent)
        { "ActorValue::kSmithing::Permanent",    { RE::ActorValue::kSmithing,         "Smithing skill level base value",         ValueType::kPermanent } },
        { "ActorValue::kAlchemy::Permanent",     { RE::ActorValue::kAlchemy,          "Alchemy skill level base value",          ValueType::kPermanent } },
        { "ActorValue::kEnchanting::Permanent",  { RE::ActorValue::kEnchanting,       "Enchanting skill level base value",       ValueType::kPermanent } },

        // Armor skills
        { "ActorValue::kHeavyArmor",       { RE::ActorValue::kHeavyArmor,       "Heavy Armor skill level",                  ValueType::kCurrent   } },
        { "ActorValue::kLightArmor",       { RE::ActorValue::kLightArmor,       "Light Armor skill level",                  ValueType::kCurrent   } },

        // Armor skills (Permanent)
        { "ActorValue::kHeavyArmor::Permanent", { RE::ActorValue::kHeavyArmor,       "Heavy Armor skill level base value",      ValueType::kPermanent } },
        { "ActorValue::kLightArmor::Permanent", { RE::ActorValue::kLightArmor,       "Light Armor skill level base value",      ValueType::kPermanent } },

        // Stealth skills
        { "ActorValue::kPickpocket",       { RE::ActorValue::kPickpocket,       "Pickpocket skill level",                   ValueType::kCurrent   } },
        { "ActorValue::kLockpicking",      { RE::ActorValue::kLockpicking,      "Lockpicking skill level",                  ValueType::kCurrent   } },
        { "ActorValue::kSneak",            { RE::ActorValue::kSneak,            "Sneak skill level",                        ValueType::kCurrent   } },
        { "ActorValue::kSpeech",           { RE::ActorValue::kSpeech,           "Speech skill level",                       ValueType::kCurrent   } },

        // Stealth skills (Permanent)
        { "ActorValue::kPickpocket::Permanent",  { RE::ActorValue::kPickpocket,       "Pickpocket skill level base value",       ValueType::kPermanent } },
        { "ActorValue::kLockpicking::Permanent", { RE::ActorValue::kLockpicking,      "Lockpicking skill level base value",      ValueType::kPermanent } },
        { "ActorValue::kSneak::Permanent",       { RE::ActorValue::kSneak,            "Sneak skill level base value",            ValueType::kPermanent } },
        { "ActorValue::kSpeech::Permanent",      { RE::ActorValue::kSpeech,           "Speech skill level base value",           ValueType::kPermanent } },

        // Magic skills
        { "ActorValue::kAlteration",       { RE::ActorValue::kAlteration,       "Alteration skill level",                   ValueType::kCurrent   } },
        { "ActorValue::kConjuration",      { RE::ActorValue::kConjuration,      "Conjuration skill level",                  ValueType::kCurrent   } },
        { "ActorValue::kDestruction",      { RE::ActorValue::kDestruction,      "Destruction skill level",                  ValueType::kCurrent   } },
        { "ActorValue::kIllusion",         { RE::ActorValue::kIllusion,         "Illusion skill level",                     ValueType::kCurrent   } },
        { "ActorValue::kRestoration",      { RE::ActorValue::kRestoration,      "Restoration skill level",                  ValueType::kCurrent   } },

        // Magic skills (Permanent)
        { "ActorValue::kAlteration::Permanent",    { RE::ActorValue::kAlteration,       "Alteration skill level base value",       ValueType::kPermanent } },
        { "ActorValue::kConjuration::Permanent",   { RE::ActorValue::kConjuration,      "Conjuration skill level base value",      ValueType::kPermanent } },
        { "ActorValue::kDestruction::Permanent",   { RE::ActorValue::kDestruction,      "Destruction skill level base value",      ValueType::kPermanent } },
        { "ActorValue::kIllusion::Permanent",      { RE::ActorValue::kIllusion,         "Illusion skill level base value",         ValueType::kPermanent } },
        { "ActorValue::kRestoration::Permanent",   { RE::ActorValue::kRestoration,      "Restoration skill level base value",      ValueType::kPermanent } },

        // Dragon
        { "ActorValue::kDragonSouls",      { RE::ActorValue::kDragonSouls,      "Collected dragon souls",                   ValueType::kCurrent   } },
        { "ActorValue::kShoutRecoveryMult",{ RE::ActorValue::kShoutRecoveryMult,"Shout recovery multiplier",               ValueType::kCurrent   } },

        // Dragon (Permanent)
        { "ActorValue::kDragonSouls::Permanent",      { RE::ActorValue::kDragonSouls,       "Collected dragon souls base value",         ValueType::kPermanent } },
        { "ActorValue::kShoutRecoveryMult::Permanent",{ RE::ActorValue::kShoutRecoveryMult, "Shout recovery multiplier base value",     ValueType::kPermanent } },
    };
    // clang-format on

    const std::unordered_map<std::string, Entry>& GetAll()
    {
        return s_registry;
    }

    std::optional<Entry> Resolve(const std::string& key)
    {
        auto it = s_registry.find(key);
        if (it == s_registry.end())
            return std::nullopt;
        return it->second;
    }

    std::string BuildDescribeJson()
    {
        nlohmann::json result;
        result["type"] = "describe";
        auto& fields   = result["fields"];
        for (auto& [key, entry] : s_registry) {
            std::string valueTypeStr = (entry.valueType == ValueType::kPermanent) ? "permanent" : "current";
            fields[key] = { { "valueType", "float" }, { "valueCategory", valueTypeStr }, { "description", entry.description } };
        }
        return result.dump();
    }
}
