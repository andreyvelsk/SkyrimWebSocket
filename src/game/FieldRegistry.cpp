#include "FieldRegistry.h"
#include "HotkeyReader.h"
#include "InventoryReader.h"
#include "MagicReader.h"
#include "PlayerReader.h"

#include <nlohmann/json.hpp>

namespace FieldRegistry
{
    // clang-format off
    static const std::unordered_map<std::string, Entry> s_registry = {
        // Vitals (All Types)
        { "ActorValue::kHealth",              { RE::ActorValue::kHealth,           "Current health points",                       ValueType::kCurrent   } },
        { "ActorValue::kHealth::Base",        { RE::ActorValue::kHealth,           "Base health points",                          ValueType::kBase      } },
        { "ActorValue::kHealth::Permanent",   { RE::ActorValue::kHealth,           "Permanent health points base value",           ValueType::kPermanent } },
        { "ActorValue::kHealth::Clamped",     { RE::ActorValue::kHealth,           "Clamped health points (0 to max)",             ValueType::kClamped   } },
        { "ActorValue::kMagicka",             { RE::ActorValue::kMagicka,          "Current magicka points",                      ValueType::kCurrent   } },
        { "ActorValue::kMagicka::Base",       { RE::ActorValue::kMagicka,          "Base magicka points",                         ValueType::kBase      } },
        { "ActorValue::kMagicka::Permanent",  { RE::ActorValue::kMagicka,          "Permanent magicka points base value",          ValueType::kPermanent } },
        { "ActorValue::kMagicka::Clamped",    { RE::ActorValue::kMagicka,          "Clamped magicka points (0 to max)",            ValueType::kClamped   } },
        { "ActorValue::kStamina",             { RE::ActorValue::kStamina,          "Current stamina points",                      ValueType::kCurrent   } },
        { "ActorValue::kStamina::Base",       { RE::ActorValue::kStamina,          "Base stamina points",                         ValueType::kBase      } },
        { "ActorValue::kStamina::Permanent",  { RE::ActorValue::kStamina,          "Permanent stamina points base value",          ValueType::kPermanent } },
        { "ActorValue::kStamina::Clamped",    { RE::ActorValue::kStamina,          "Clamped stamina points (0 to max)",            ValueType::kClamped   } },

        // Regeneration rates (All Types)
        { "ActorValue::kHealRate",                 { RE::ActorValue::kHealRate,         "Health regeneration rate",              ValueType::kCurrent   } },
        { "ActorValue::kHealRate::Base",           { RE::ActorValue::kHealRate,         "Base health regeneration rate",         ValueType::kBase      } },
        { "ActorValue::kHealRate::Permanent",      { RE::ActorValue::kHealRate,         "Permanent health regeneration rate",    ValueType::kPermanent } },
        { "ActorValue::kHealRate::Clamped",        { RE::ActorValue::kHealRate,         "Clamped health regeneration rate",      ValueType::kClamped   } },
        { "ActorValue::kMagickaRate",              { RE::ActorValue::kMagickaRate,      "Magicka regeneration rate",             ValueType::kCurrent   } },
        { "ActorValue::kMagickaRate::Base",        { RE::ActorValue::kMagickaRate,      "Base magicka regeneration rate",        ValueType::kBase      } },
        { "ActorValue::kMagickaRate::Permanent",   { RE::ActorValue::kMagickaRate,      "Permanent magicka regeneration rate",   ValueType::kPermanent } },
        { "ActorValue::kMagickaRate::Clamped",     { RE::ActorValue::kMagickaRate,      "Clamped magicka regeneration rate",     ValueType::kClamped   } },
        { "ActorValue::kStaminaRate",              { RE::ActorValue::kStaminaRate,      "Stamina regeneration rate",             ValueType::kCurrent   } },
        { "ActorValue::kStaminaRate::Base",        { RE::ActorValue::kStaminaRate,      "Base stamina regeneration rate",        ValueType::kBase      } },
        { "ActorValue::kStaminaRate::Permanent",   { RE::ActorValue::kStaminaRate,      "Permanent stamina regeneration rate",   ValueType::kPermanent } },
        { "ActorValue::kStaminaRate::Clamped",     { RE::ActorValue::kStaminaRate,      "Clamped stamina regeneration rate",     ValueType::kClamped   } },
        { "ActorValue::kHealRateMult",             { RE::ActorValue::kHealRateMult,     "Health regeneration multiplier",        ValueType::kCurrent   } },
        { "ActorValue::kHealRateMult::Base",       { RE::ActorValue::kHealRateMult,     "Base health regeneration multiplier",   ValueType::kBase      } },
        { "ActorValue::kHealRateMult::Permanent",  { RE::ActorValue::kHealRateMult,     "Permanent health regeneration multiplier", ValueType::kPermanent } },
        { "ActorValue::kHealRateMult::Clamped",    { RE::ActorValue::kHealRateMult,     "Clamped health regeneration multiplier", ValueType::kClamped   } },
        { "ActorValue::kMagickaRateMult",          { RE::ActorValue::kMagickaRateMult,  "Magicka regeneration multiplier",       ValueType::kCurrent   } },
        { "ActorValue::kMagickaRateMult::Base",    { RE::ActorValue::kMagickaRateMult,  "Base magicka regeneration multiplier",  ValueType::kBase      } },
        { "ActorValue::kMagickaRateMult::Permanent", { RE::ActorValue::kMagickaRateMult, "Permanent magicka regeneration multiplier", ValueType::kPermanent } },
        { "ActorValue::kMagickaRateMult::Clamped",  { RE::ActorValue::kMagickaRateMult, "Clamped magicka regeneration multiplier", ValueType::kClamped   } },
        { "ActorValue::kStaminaRateMult",          { RE::ActorValue::kStaminaRateMult,  "Stamina regeneration multiplier",       ValueType::kCurrent   } },
        { "ActorValue::kStaminaRateMult::Base",    { RE::ActorValue::kStaminaRateMult,  "Base stamina regeneration multiplier",  ValueType::kBase      } },
        { "ActorValue::kStaminaRateMult::Permanent", { RE::ActorValue::kStaminaRateMult, "Permanent stamina regeneration multiplier", ValueType::kPermanent } },
        { "ActorValue::kStaminaRateMult::Clamped",  { RE::ActorValue::kStaminaRateMult, "Clamped stamina regeneration multiplier", ValueType::kClamped   } },

        // Movement & inventory (All Types)
        { "ActorValue::kSpeedMult",            { RE::ActorValue::kSpeedMult,        "Movement speed multiplier",             ValueType::kCurrent   } },
        { "ActorValue::kSpeedMult::Base",      { RE::ActorValue::kSpeedMult,        "Base movement speed multiplier",        ValueType::kBase      } },
        { "ActorValue::kSpeedMult::Permanent", { RE::ActorValue::kSpeedMult,        "Permanent movement speed multiplier",   ValueType::kPermanent } },
        { "ActorValue::kSpeedMult::Clamped",   { RE::ActorValue::kSpeedMult,        "Clamped movement speed multiplier",     ValueType::kClamped   } },
        { "ActorValue::kCarryWeight",          { RE::ActorValue::kCarryWeight,      "Carry weight capacity",                  ValueType::kCurrent   } },
        { "ActorValue::kCarryWeight::Base",    { RE::ActorValue::kCarryWeight,      "Base carry weight capacity",             ValueType::kBase      } },
        { "ActorValue::kCarryWeight::Permanent", { RE::ActorValue::kCarryWeight,      "Permanent carry weight capacity",        ValueType::kPermanent } },
        { "ActorValue::kCarryWeight::Clamped",   { RE::ActorValue::kCarryWeight,      "Clamped carry weight capacity",          ValueType::kClamped   } },

        // Combat (All Types)
        { "ActorValue::kAttackDamageMult",          { RE::ActorValue::kAttackDamageMult, "Attack damage multiplier",            ValueType::kCurrent   } },
        { "ActorValue::kAttackDamageMult::Base",    { RE::ActorValue::kAttackDamageMult, "Base attack damage multiplier",       ValueType::kBase      } },
        { "ActorValue::kAttackDamageMult::Permanent", { RE::ActorValue::kAttackDamageMult, "Permanent attack damage multiplier",   ValueType::kPermanent } },
        { "ActorValue::kAttackDamageMult::Clamped",   { RE::ActorValue::kAttackDamageMult, "Clamped attack damage multiplier",     ValueType::kClamped   } },
        { "ActorValue::kCriticalChance",         { RE::ActorValue::kCriticalChance,   "Critical hit chance",                   ValueType::kCurrent   } },
        { "ActorValue::kCriticalChance::Base",   { RE::ActorValue::kCriticalChance,   "Base critical hit chance",              ValueType::kBase      } },
        { "ActorValue::kCriticalChance::Permanent", { RE::ActorValue::kCriticalChance,   "Permanent critical hit chance",       ValueType::kPermanent } },
        { "ActorValue::kCriticalChance::Clamped",   { RE::ActorValue::kCriticalChance,   "Clamped critical hit chance",         ValueType::kClamped   } },

        // Resistances (All Types)
        { "ActorValue::kDamageResist",          { RE::ActorValue::kDamageResist,     "Physical damage resistance",            ValueType::kCurrent   } },
        { "ActorValue::kDamageResist::Base",    { RE::ActorValue::kDamageResist,     "Base physical damage resistance",       ValueType::kBase      } },
        { "ActorValue::kDamageResist::Permanent", { RE::ActorValue::kDamageResist,     "Permanent physical damage resistance",  ValueType::kPermanent } },
        { "ActorValue::kDamageResist::Clamped",   { RE::ActorValue::kDamageResist,     "Clamped physical damage resistance",    ValueType::kClamped   } },
        { "ActorValue::kResistMagic",           { RE::ActorValue::kResistMagic,      "Magic resistance",                      ValueType::kCurrent   } },
        { "ActorValue::kResistMagic::Base",     { RE::ActorValue::kResistMagic,      "Base magic resistance",                 ValueType::kBase      } },
        { "ActorValue::kResistMagic::Permanent", { RE::ActorValue::kResistMagic,      "Permanent magic resistance",             ValueType::kPermanent } },
        { "ActorValue::kResistMagic::Clamped",   { RE::ActorValue::kResistMagic,      "Clamped magic resistance",               ValueType::kClamped   } },
        { "ActorValue::kResistFire",            { RE::ActorValue::kResistFire,       "Fire resistance",                       ValueType::kCurrent   } },
        { "ActorValue::kResistFire::Base",      { RE::ActorValue::kResistFire,       "Base fire resistance",                  ValueType::kBase      } },
        { "ActorValue::kResistFire::Permanent", { RE::ActorValue::kResistFire,       "Permanent fire resistance",              ValueType::kPermanent } },
        { "ActorValue::kResistFire::Clamped",   { RE::ActorValue::kResistFire,       "Clamped fire resistance",                ValueType::kClamped   } },
        { "ActorValue::kResistFrost",           { RE::ActorValue::kResistFrost,      "Frost resistance",                      ValueType::kCurrent   } },
        { "ActorValue::kResistFrost::Base",     { RE::ActorValue::kResistFrost,      "Base frost resistance",                 ValueType::kBase      } },
        { "ActorValue::kResistFrost::Permanent", { RE::ActorValue::kResistFrost,      "Permanent frost resistance",             ValueType::kPermanent } },
        { "ActorValue::kResistFrost::Clamped",   { RE::ActorValue::kResistFrost,      "Clamped frost resistance",               ValueType::kClamped   } },
        { "ActorValue::kResistShock",           { RE::ActorValue::kResistShock,      "Shock resistance",                      ValueType::kCurrent   } },
        { "ActorValue::kResistShock::Base",     { RE::ActorValue::kResistShock,      "Base shock resistance",                 ValueType::kBase      } },
        { "ActorValue::kResistShock::Permanent", { RE::ActorValue::kResistShock,      "Permanent shock resistance",             ValueType::kPermanent } },
        { "ActorValue::kResistShock::Clamped",   { RE::ActorValue::kResistShock,      "Clamped shock resistance",               ValueType::kClamped   } },
        { "ActorValue::kPoisonResist",          { RE::ActorValue::kPoisonResist,     "Poison resistance",                     ValueType::kCurrent   } },
        { "ActorValue::kPoisonResist::Base",    { RE::ActorValue::kPoisonResist,     "Base poison resistance",                ValueType::kBase      } },
        { "ActorValue::kPoisonResist::Permanent", { RE::ActorValue::kPoisonResist,     "Permanent poison resistance",            ValueType::kPermanent } },
        { "ActorValue::kPoisonResist::Clamped",   { RE::ActorValue::kPoisonResist,     "Clamped poison resistance",              ValueType::kClamped   } },

        // Combat skills (All Types)
        { "ActorValue::kOneHanded",             { RE::ActorValue::kOneHanded,        "One-Handed skill level",                ValueType::kCurrent   } },
        { "ActorValue::kOneHanded::Base",       { RE::ActorValue::kOneHanded,        "Base One-Handed skill level",            ValueType::kBase      } },
        { "ActorValue::kOneHanded::Permanent",  { RE::ActorValue::kOneHanded,        "One-Handed skill level base value",     ValueType::kPermanent } },
        { "ActorValue::kOneHanded::Clamped",    { RE::ActorValue::kOneHanded,        "Clamped One-Handed skill level",         ValueType::kClamped   } },
        { "ActorValue::kTwoHanded",             { RE::ActorValue::kTwoHanded,        "Two-Handed skill level",                ValueType::kCurrent   } },
        { "ActorValue::kTwoHanded::Base",       { RE::ActorValue::kTwoHanded,        "Base Two-Handed skill level",            ValueType::kBase      } },
        { "ActorValue::kTwoHanded::Permanent",  { RE::ActorValue::kTwoHanded,        "Two-Handed skill level base value",     ValueType::kPermanent } },
        { "ActorValue::kTwoHanded::Clamped",    { RE::ActorValue::kTwoHanded,        "Clamped Two-Handed skill level",         ValueType::kClamped   } },
        { "ActorValue::kArchery",               { RE::ActorValue::kArchery,          "Archery skill level",                   ValueType::kCurrent   } },
        { "ActorValue::kArchery::Base",         { RE::ActorValue::kArchery,          "Base Archery skill level",               ValueType::kBase      } },
        { "ActorValue::kArchery::Permanent",    { RE::ActorValue::kArchery,          "Archery skill level base value",         ValueType::kPermanent } },
        { "ActorValue::kArchery::Clamped",      { RE::ActorValue::kArchery,          "Clamped Archery skill level",            ValueType::kClamped   } },
        { "ActorValue::kBlock",                 { RE::ActorValue::kBlock,            "Block skill level",                     ValueType::kCurrent   } },
        { "ActorValue::kBlock::Base",           { RE::ActorValue::kBlock,            "Base Block skill level",                 ValueType::kBase      } },
        { "ActorValue::kBlock::Permanent",      { RE::ActorValue::kBlock,            "Block skill level base value",           ValueType::kPermanent } },
        { "ActorValue::kBlock::Clamped",        { RE::ActorValue::kBlock,            "Clamped Block skill level",               ValueType::kClamped   } },

        // Crafting skills (All Types)
        { "ActorValue::kSmithing",              { RE::ActorValue::kSmithing,         "Smithing skill level",                  ValueType::kCurrent   } },
        { "ActorValue::kSmithing::Base",        { RE::ActorValue::kSmithing,         "Base Smithing skill level",              ValueType::kBase      } },
        { "ActorValue::kSmithing::Permanent",   { RE::ActorValue::kSmithing,         "Smithing skill level base value",        ValueType::kPermanent } },
        { "ActorValue::kSmithing::Clamped",     { RE::ActorValue::kSmithing,         "Clamped Smithing skill level",           ValueType::kClamped   } },
        { "ActorValue::kAlchemy",               { RE::ActorValue::kAlchemy,          "Alchemy skill level",                   ValueType::kCurrent   } },
        { "ActorValue::kAlchemy::Base",         { RE::ActorValue::kAlchemy,          "Base Alchemy skill level",               ValueType::kBase      } },
        { "ActorValue::kAlchemy::Permanent",    { RE::ActorValue::kAlchemy,          "Alchemy skill level base value",         ValueType::kPermanent } },
        { "ActorValue::kAlchemy::Clamped",      { RE::ActorValue::kAlchemy,          "Clamped Alchemy skill level",            ValueType::kClamped   } },
        { "ActorValue::kEnchanting",            { RE::ActorValue::kEnchanting,       "Enchanting skill level",                ValueType::kCurrent   } },
        { "ActorValue::kEnchanting::Base",      { RE::ActorValue::kEnchanting,       "Base Enchanting skill level",            ValueType::kBase      } },
        { "ActorValue::kEnchanting::Permanent", { RE::ActorValue::kEnchanting,       "Enchanting skill level base value",     ValueType::kPermanent } },
        { "ActorValue::kEnchanting::Clamped",   { RE::ActorValue::kEnchanting,       "Clamped Enchanting skill level",         ValueType::kClamped   } },

        // Armor skills (All Types)
        { "ActorValue::kHeavyArmor",            { RE::ActorValue::kHeavyArmor,       "Heavy Armor skill level",               ValueType::kCurrent   } },
        { "ActorValue::kHeavyArmor::Base",      { RE::ActorValue::kHeavyArmor,       "Base Heavy Armor skill level",           ValueType::kBase      } },
        { "ActorValue::kHeavyArmor::Permanent", { RE::ActorValue::kHeavyArmor,       "Heavy Armor skill level base value",    ValueType::kPermanent } },
        { "ActorValue::kHeavyArmor::Clamped",   { RE::ActorValue::kHeavyArmor,       "Clamped Heavy Armor skill level",        ValueType::kClamped   } },
        { "ActorValue::kLightArmor",            { RE::ActorValue::kLightArmor,       "Light Armor skill level",               ValueType::kCurrent   } },
        { "ActorValue::kLightArmor::Base",      { RE::ActorValue::kLightArmor,       "Base Light Armor skill level",           ValueType::kBase      } },
        { "ActorValue::kLightArmor::Permanent", { RE::ActorValue::kLightArmor,       "Light Armor skill level base value",    ValueType::kPermanent } },
        { "ActorValue::kLightArmor::Clamped",   { RE::ActorValue::kLightArmor,       "Clamped Light Armor skill level",        ValueType::kClamped   } },

        // Stealth skills (All Types)
        { "ActorValue::kPickpocket",            { RE::ActorValue::kPickpocket,       "Pickpocket skill level",                ValueType::kCurrent   } },
        { "ActorValue::kPickpocket::Base",      { RE::ActorValue::kPickpocket,       "Base Pickpocket skill level",            ValueType::kBase      } },
        { "ActorValue::kPickpocket::Permanent", { RE::ActorValue::kPickpocket,       "Pickpocket skill level base value",     ValueType::kPermanent } },
        { "ActorValue::kPickpocket::Clamped",   { RE::ActorValue::kPickpocket,       "Clamped Pickpocket skill level",         ValueType::kClamped   } },
        { "ActorValue::kLockpicking",           { RE::ActorValue::kLockpicking,      "Lockpicking skill level",               ValueType::kCurrent   } },
        { "ActorValue::kLockpicking::Base",     { RE::ActorValue::kLockpicking,      "Base Lockpicking skill level",           ValueType::kBase      } },
        { "ActorValue::kLockpicking::Permanent", { RE::ActorValue::kLockpicking,      "Lockpicking skill level base value",    ValueType::kPermanent } },
        { "ActorValue::kLockpicking::Clamped",   { RE::ActorValue::kLockpicking,      "Clamped Lockpicking skill level",        ValueType::kClamped   } },
        { "ActorValue::kSneak",                 { RE::ActorValue::kSneak,            "Sneak skill level",                     ValueType::kCurrent   } },
        { "ActorValue::kSneak::Base",           { RE::ActorValue::kSneak,            "Base Sneak skill level",                 ValueType::kBase      } },
        { "ActorValue::kSneak::Permanent",      { RE::ActorValue::kSneak,            "Sneak skill level base value",           ValueType::kPermanent } },
        { "ActorValue::kSneak::Clamped",        { RE::ActorValue::kSneak,            "Clamped Sneak skill level",               ValueType::kClamped   } },
        { "ActorValue::kSpeech",                { RE::ActorValue::kSpeech,           "Speech skill level",                    ValueType::kCurrent   } },
        { "ActorValue::kSpeech::Base",          { RE::ActorValue::kSpeech,           "Base Speech skill level",                ValueType::kBase      } },
        { "ActorValue::kSpeech::Permanent",     { RE::ActorValue::kSpeech,           "Speech skill level base value",          ValueType::kPermanent } },
        { "ActorValue::kSpeech::Clamped",       { RE::ActorValue::kSpeech,           "Clamped Speech skill level",              ValueType::kClamped   } },

        // Magic skills (All Types)
        { "ActorValue::kAlteration",            { RE::ActorValue::kAlteration,       "Alteration skill level",                ValueType::kCurrent   } },
        { "ActorValue::kAlteration::Base",      { RE::ActorValue::kAlteration,       "Base Alteration skill level",            ValueType::kBase      } },
        { "ActorValue::kAlteration::Permanent", { RE::ActorValue::kAlteration,       "Alteration skill level base value",     ValueType::kPermanent } },
        { "ActorValue::kAlteration::Clamped",   { RE::ActorValue::kAlteration,       "Clamped Alteration skill level",         ValueType::kClamped   } },
        { "ActorValue::kConjuration",           { RE::ActorValue::kConjuration,      "Conjuration skill level",               ValueType::kCurrent   } },
        { "ActorValue::kConjuration::Base",     { RE::ActorValue::kConjuration,      "Base Conjuration skill level",           ValueType::kBase      } },
        { "ActorValue::kConjuration::Permanent", { RE::ActorValue::kConjuration,      "Conjuration skill level base value",    ValueType::kPermanent } },
        { "ActorValue::kConjuration::Clamped",   { RE::ActorValue::kConjuration,      "Clamped Conjuration skill level",        ValueType::kClamped   } },
        { "ActorValue::kDestruction",           { RE::ActorValue::kDestruction,      "Destruction skill level",               ValueType::kCurrent   } },
        { "ActorValue::kDestruction::Base",     { RE::ActorValue::kDestruction,      "Base Destruction skill level",           ValueType::kBase      } },
        { "ActorValue::kDestruction::Permanent", { RE::ActorValue::kDestruction,      "Destruction skill level base value",    ValueType::kPermanent } },
        { "ActorValue::kDestruction::Clamped",   { RE::ActorValue::kDestruction,      "Clamped Destruction skill level",        ValueType::kClamped   } },
        { "ActorValue::kIllusion",              { RE::ActorValue::kIllusion,         "Illusion skill level",                  ValueType::kCurrent   } },
        { "ActorValue::kIllusion::Base",        { RE::ActorValue::kIllusion,         "Base Illusion skill level",              ValueType::kBase      } },
        { "ActorValue::kIllusion::Permanent",   { RE::ActorValue::kIllusion,         "Illusion skill level base value",        ValueType::kPermanent } },
        { "ActorValue::kIllusion::Clamped",     { RE::ActorValue::kIllusion,         "Clamped Illusion skill level",           ValueType::kClamped   } },
        { "ActorValue::kRestoration",           { RE::ActorValue::kRestoration,      "Restoration skill level",               ValueType::kCurrent   } },
        { "ActorValue::kRestoration::Base",     { RE::ActorValue::kRestoration,      "Base Restoration skill level",           ValueType::kBase      } },
        { "ActorValue::kRestoration::Permanent", { RE::ActorValue::kRestoration,      "Restoration skill level base value",    ValueType::kPermanent } },
        { "ActorValue::kRestoration::Clamped",   { RE::ActorValue::kRestoration,      "Clamped Restoration skill level",        ValueType::kClamped   } },

        // Dragon (All Types)
        { "ActorValue::kDragonSouls",           { RE::ActorValue::kDragonSouls,      "Collected dragon souls",                ValueType::kCurrent   } },
        { "ActorValue::kDragonSouls::Base",     { RE::ActorValue::kDragonSouls,      "Base dragon souls",                     ValueType::kBase      } },
        { "ActorValue::kDragonSouls::Permanent", { RE::ActorValue::kDragonSouls,      "Collected dragon souls base value",     ValueType::kPermanent } },
        { "ActorValue::kDragonSouls::Clamped",   { RE::ActorValue::kDragonSouls,      "Clamped dragon souls",                  ValueType::kClamped   } },
        { "ActorValue::kShoutRecoveryMult",     { RE::ActorValue::kShoutRecoveryMult,"Shout recovery multiplier",             ValueType::kCurrent   } },
        { "ActorValue::kShoutRecoveryMult::Base", { RE::ActorValue::kShoutRecoveryMult, "Base shout recovery multiplier",       ValueType::kBase      } },
        { "ActorValue::kShoutRecoveryMult::Permanent", { RE::ActorValue::kShoutRecoveryMult, "Shout recovery multiplier base value", ValueType::kPermanent } },
        { "ActorValue::kShoutRecoveryMult::Clamped",   { RE::ActorValue::kShoutRecoveryMult, "Clamped shout recovery multiplier",   ValueType::kClamped   } },
    };

    // clang-format off
    static const std::unordered_map<std::string, JsonEntry> s_json_registry = {
        // Inventory categories
        { "Inventory::Categories",
          { "Array of inventory categories with item counts", "array",
            &InventoryReader::ReadCategories } },

        // Gold
        { "Inventory::Gold",
          { "Player's current gold amount", "integer",
            &InventoryReader::ReadGold } },

        // Inventory items by category
        { "Inventory::Items::Weapons",
          { "Weapons in player inventory", "array",
            &InventoryReader::ReadWeapons } },
        { "Inventory::Items::Apparel",
          { "Apparel in player inventory", "array",
            &InventoryReader::ReadApparel } },
        { "Inventory::Items::Books",
          { "Books in player inventory", "array",
            &InventoryReader::ReadBooks } },
        { "Inventory::Items::Potions",
          { "Potions in player inventory (food excluded)", "array",
            &InventoryReader::ReadPotions } },
        { "Inventory::Items::Food",
          { "Food items in player inventory", "array",
            &InventoryReader::ReadFood } },
        { "Inventory::Items::Ingredients",
          { "Ingredients in player inventory", "array",
            &InventoryReader::ReadIngredients } },
        { "Inventory::Items::Misc",
          { "Miscellaneous items in player inventory (gold excluded)", "array",
            &InventoryReader::ReadMisc } },
        { "Inventory::Items::Ammo",
          { "Ammunition in player inventory", "array",
            InventoryReader::MakeItemsResolver(RE::FormType::Ammo) } },
        { "Inventory::Items::Keys",
          { "Keys in player inventory", "array",
            InventoryReader::MakeItemsResolver(RE::FormType::KeyMaster) } },
        { "Inventory::Items::SoulGems",
          { "Soul gems in player inventory", "array",
            &InventoryReader::ReadSoulGems } },
        { "Inventory::Items::Scrolls",
          { "Scrolls in player inventory", "array",
            &InventoryReader::ReadScrolls } },
        { "Inventory::Items::Favorites",
          { "Favorited items across all categories", "array",
            &InventoryReader::ReadFavorites } },

        // Magic schools and spells
        { "Magic::Categories",
          { "Array of magic schools with spell counts", "array",
            &MagicReader::ReadCategories } },

        { "Magic::Items::Destruction",
          { "Destruction spells known by player", "array",
            &MagicReader::ReadDestruction } },
        { "Magic::Items::Alteration",
          { "Alteration spells known by player", "array",
            &MagicReader::ReadAlteration } },
        { "Magic::Items::Conjuration",
          { "Conjuration spells known by player", "array",
            &MagicReader::ReadConjuration } },
        { "Magic::Items::Illusion",
          { "Illusion spells known by player", "array",
            &MagicReader::ReadIllusion } },
        { "Magic::Items::Restoration",
          { "Restoration spells known by player", "array",
            &MagicReader::ReadRestoration } },
        { "Magic::Items::Enchanting",
          { "Enchanting spells known by player", "array",
            &MagicReader::ReadEnchanting } },

        // Hotkeys
        { "Hotkey::Items",
          { "All 8 hotkey slots with their current bindings (spells, shouts, powers, or items)", "array",
            &HotkeyReader::ReadItems } },

        // Player stats
        { "Player::Level",
          { "Character level", "integer",
            &PlayerReader::ReadLevel } },
        { "Player::XP::Current",
          { "XP earned within the current level (resets to 0 on level-up)", "float",
            &PlayerReader::ReadXPCurrent } },
        { "Player::XP::Next",
          { "XP threshold required to reach the next level", "float",
            &PlayerReader::ReadXPNext } },
        { "Player::XP::LevelStart",
          { "XP value at the start of the current level (always 0; provided for progress-bar math alongside XP::Current and XP::Next)", "float",
            &PlayerReader::ReadXPLevelStart } },
        { "Player::InventoryWeight",
          { "Total weight of all items currently in the player's inventory", "float",
            &PlayerReader::ReadInventoryWeight } },
        { "Player::CarryWeight",
          { "Maximum carry weight (same value as ActorValue::kCarryWeight)", "float",
            &PlayerReader::ReadCarryWeight } },

        // Game settings
        { "Game::Language",
          { "Current game language from sLanguage:General INI setting (e.g. \"english\", \"russian\")", "string",
            &PlayerReader::ReadLanguage, /*requiresInGame=*/false } },

        // Game / player runtime state
        { "Game::Status",
          { "Current game/player state flags: paused, loading, inMainMenu, inDialogue, inCombat, dead, controlsEnabled, canAct", "object",
            &PlayerReader::ReadGameStatus, /*requiresInGame=*/false } },

        // Player position and heading
        { "Player::Position",
          { "Player position and heading in the current worldspace/cell: { x, y, z, angle, worldspace, worldspaceFormId, parentWorldspace, parentWorldspaceFormId, cell, cellFormId, isInterior }. x/y/z are local to the current worldspace (or interior); worldspace fields are null in interiors. parentWorldspace walks up to the root world (e.g. Tamriel for city sub-worlds).", "object",
            &PlayerReader::ReadPosition } },

        // Last known exterior position (for placing the player on the global
        // map while inside an interior or a city sub-worldspace)
        { "Player::ExteriorPosition",
          { "Last known exterior position cached by the game: { x, y, z, worldspace, worldspaceFormId, parentWorldspace, parentWorldspaceFormId }. Updated whenever the player is in an exterior cell; remains valid while in interiors. Useful for showing the player on a global (parent) worldspace map.", "object",
            &PlayerReader::ReadExteriorPosition } },

        // Map markers
        { "Map::Markers::Locations",
          { "Array of map markers currently shown on the player's world map (only entries with isVisible=true — i.e. pre-discovered cities, locations the player has discovered, and quest-script-revealed markers). Each entry: { refId, name, type, typeId, x, y, isVisible, canFastTravel }.",
            "array",
            &PlayerReader::ReadMapMarkers } },
        { "Map::Markers::All",
          { "Array of ALL map markers in every loaded worldspace, including undiscovered/hidden ones. Same entry shape as Map::Markers::Locations.",
            "array",
            &PlayerReader::ReadMapMarkersAll } },
        { "Map::Markers::Quests",
          { "Array of active quest-marker destinations (the markers Skyrim renders as the floating quest arrows on the compass and as quest-target icons on the world map). On SE/AE this uses PlayerCharacter::questTargets plus TESQuest::IsActive()/QuestFlag::kActive, the journal tracking bit set through SetActiveQuest / the journal UI; Miscellaneous targets also honor the journal's master Miscellaneous toggle when observed from the journal Scaleform list or native fallback. When a target belongs to a BGSLocation, x/y/z are resolved through a map-facing location marker: BGSLocation::worldLocMarker when usable, otherwise location specialRefs / persistent-cell ExtraMapMarker refs matching Map::Markers::Locations. localX/localY/localZ keep the raw target reference coordinates; unresolved interior or child-worldspace targets return null x/y/z instead of local coordinates. Alternative target aliases resolving to the same marker are collapsed. Each entry: { questFormId, questEditorId, questName, questType, isActive, isMiscellaneous, objectiveIndex, objectiveText, objectiveTextResolved, aliasId, refId, isDeleted, name, coordinateSource, coordinateRefId, coordinateRefName, locationFormId, locationEditorId, locationName, localX, localY, localZ, localWorldspace, localWorldspaceFormId, localParentWorldspace, localParentWorldspaceFormId, localCell, localCellFormId, localIsInterior, x, y, z, worldspace, worldspaceFormId, parentWorldspace, parentWorldspaceFormId, cell, cellFormId, isInterior }. objectiveText keeps the raw template (may contain <Alias=...> placeholders for radiant quests); objectiveTextResolved resolves those aliases through the current quest instance data where possible.",
            "array",
            &PlayerReader::ReadQuestMarkers } },
        { "Debug::Map::Markers::Quests",
          { "Debug snapshot for quest marker diagnosis: runtime module, Miscellaneous master-toggle state (including Scaleform/native source), questTargets with coordinate candidates, runtime objectives, static active/displayed objectives, and current Map::Markers::Quests output.",
            "object",
            &PlayerReader::ReadQuestMarkersDebug } },

        // Player-placed custom map marker (the marker the player drops on
        // the world map by clicking on it).
        { "Player::Marker",
          { "Player-placed custom map marker state: { isSet, x, y, z, worldspace, worldspaceFormId, parentWorldspace, parentWorldspaceFormId }. When the player has not placed a marker (or has cleared it), isSet=false and spatial fields are null. Use the player_marker_set / player_marker_clear commands to modify it.",
            "object",
            &PlayerReader::ReadPlayerMarker } },
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

    std::optional<JsonEntry> ResolveJson(const std::string& key)
    {
        auto it = s_json_registry.find(key);
        if (it == s_json_registry.end())
            return std::nullopt;
        return it->second;
    }

    bool IsInGame()
    {
        // Main menu open => definitely not in a save.
        if (auto* ui = RE::UI::GetSingleton()) {
            if (ui->IsMenuOpen(RE::MainMenu::MENU_NAME))
                return false;
        }
        // Player must exist and be in a loaded cell. parentCell is null on
        // the main menu and during the load screen between saves.
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !player->GetParentCell())
            return false;
        return true;
    }
}
