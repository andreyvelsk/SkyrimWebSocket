# ActorValue Field Reference

This document lists all available ActorValue fields that can be queried through the WebSocket plugin.

## Value Type Modifiers

Each ActorValue can be queried with different value type modifiers:

- **No suffix** (or `::kCurrent`) — Current value with temporary modifiers applied
- **`::Base`** — Explicit base value without modifications
- **`::Permanent`** — Permanent base value (similar to Base in most cases)
- **`::Clamped`** — Value clamped to valid min/max ranges (useful for UI display)

### Example:
```json
{
  "type": "query",
  "fields": {
    "health_current":   "ActorValue::kHealth",
    "health_base":      "ActorValue::kHealth::Base",
    "health_permanent": "ActorValue::kHealth::Permanent",
    "health_clamped":   "ActorValue::kHealth::Clamped"
  }
}
```

---

## Vitals (Health, Magicka, Stamina)

| Field | Description |
|-------|-------------|
| `ActorValue::kHealth` | Current health points |
| `ActorValue::kHealth::Base` | Base health points |
| `ActorValue::kHealth::Permanent` | Permanent health points base value |
| `ActorValue::kHealth::Clamped` | Clamped health points (0 to max) |
| `ActorValue::kMagicka` | Current magicka points |
| `ActorValue::kMagicka::Base` | Base magicka points |
| `ActorValue::kMagicka::Permanent` | Permanent magicka points base value |
| `ActorValue::kMagicka::Clamped` | Clamped magicka points (0 to max) |
| `ActorValue::kStamina` | Current stamina points |
| `ActorValue::kStamina::Base` | Base stamina points |
| `ActorValue::kStamina::Permanent` | Permanent stamina points base value |
| `ActorValue::kStamina::Clamped` | Clamped stamina points (0 to max) |

---

## Regeneration Rates

| Field | Description |
|-------|-------------|
| `ActorValue::kHealRate` | Health regeneration rate |
| `ActorValue::kHealRate::Base` | Base health regeneration rate |
| `ActorValue::kHealRate::Permanent` | Permanent health regeneration rate |
| `ActorValue::kHealRate::Clamped` | Clamped health regeneration rate |
| `ActorValue::kMagickaRate` | Magicka regeneration rate |
| `ActorValue::kMagickaRate::Base` | Base magicka regeneration rate |
| `ActorValue::kMagickaRate::Permanent` | Permanent magicka regeneration rate |
| `ActorValue::kMagickaRate::Clamped` | Clamped magicka regeneration rate |
| `ActorValue::kStaminaRate` | Stamina regeneration rate |
| `ActorValue::kStaminaRate::Base` | Base stamina regeneration rate |
| `ActorValue::kStaminaRate::Permanent` | Permanent stamina regeneration rate |
| `ActorValue::kStaminaRate::Clamped` | Clamped stamina regeneration rate |

---

## Regeneration Multipliers

| Field | Description |
|-------|-------------|
| `ActorValue::kHealRateMult` | Health regeneration multiplier |
| `ActorValue::kHealRateMult::Base` | Base health regeneration multiplier |
| `ActorValue::kHealRateMult::Permanent` | Permanent health regeneration multiplier |
| `ActorValue::kHealRateMult::Clamped` | Clamped health regeneration multiplier |
| `ActorValue::kMagickaRateMult` | Magicka regeneration multiplier |
| `ActorValue::kMagickaRateMult::Base` | Base magicka regeneration multiplier |
| `ActorValue::kMagickaRateMult::Permanent` | Permanent magicka regeneration multiplier |
| `ActorValue::kMagickaRateMult::Clamped` | Clamped magicka regeneration multiplier |
| `ActorValue::kStaminaRateMult` | Stamina regeneration multiplier |
| `ActorValue::kStaminaRateMult::Base` | Base stamina regeneration multiplier |
| `ActorValue::kStaminaRateMult::Permanent` | Permanent stamina regeneration multiplier |
| `ActorValue::kStaminaRateMult::Clamped` | Clamped stamina regeneration multiplier |

---

## Movement & Carry Weight

| Field | Description |
|-------|-------------|
| `ActorValue::kSpeedMult` | Movement speed multiplier |
| `ActorValue::kSpeedMult::Base` | Base movement speed multiplier |
| `ActorValue::kSpeedMult::Permanent` | Permanent movement speed multiplier |
| `ActorValue::kSpeedMult::Clamped` | Clamped movement speed multiplier |
| `ActorValue::kCarryWeight` | Carry weight capacity |
| `ActorValue::kCarryWeight::Base` | Base carry weight capacity |
| `ActorValue::kCarryWeight::Permanent` | Permanent carry weight capacity |
| `ActorValue::kCarryWeight::Clamped` | Clamped carry weight capacity |

---

## Combat

| Field | Description |
|-------|-------------|
| `ActorValue::kAttackDamageMult` | Attack damage multiplier |
| `ActorValue::kAttackDamageMult::Base` | Base attack damage multiplier |
| `ActorValue::kAttackDamageMult::Permanent` | Permanent attack damage multiplier |
| `ActorValue::kAttackDamageMult::Clamped` | Clamped attack damage multiplier |
| `ActorValue::kCriticalChance` | Critical hit chance |
| `ActorValue::kCriticalChance::Base` | Base critical hit chance |
| `ActorValue::kCriticalChance::Permanent` | Permanent critical hit chance |
| `ActorValue::kCriticalChance::Clamped` | Clamped critical hit chance |

---

## Resistances

| Field | Description |
|-------|-------------|
| `ActorValue::kDamageResist` | Physical damage resistance |
| `ActorValue::kDamageResist::Base` | Base physical damage resistance |
| `ActorValue::kDamageResist::Permanent` | Permanent physical damage resistance |
| `ActorValue::kDamageResist::Clamped` | Clamped physical damage resistance |
| `ActorValue::kResistMagic` | Magic resistance |
| `ActorValue::kResistMagic::Base` | Base magic resistance |
| `ActorValue::kResistMagic::Permanent` | Permanent magic resistance |
| `ActorValue::kResistMagic::Clamped` | Clamped magic resistance |
| `ActorValue::kResistFire` | Fire resistance |
| `ActorValue::kResistFire::Base` | Base fire resistance |
| `ActorValue::kResistFire::Permanent` | Permanent fire resistance |
| `ActorValue::kResistFire::Clamped` | Clamped fire resistance |
| `ActorValue::kResistFrost` | Frost resistance |
| `ActorValue::kResistFrost::Base` | Base frost resistance |
| `ActorValue::kResistFrost::Permanent` | Permanent frost resistance |
| `ActorValue::kResistFrost::Clamped` | Clamped frost resistance |
| `ActorValue::kResistShock` | Shock resistance |
| `ActorValue::kResistShock::Base` | Base shock resistance |
| `ActorValue::kResistShock::Permanent` | Permanent shock resistance |
| `ActorValue::kResistShock::Clamped` | Clamped shock resistance |
| `ActorValue::kPoisonResist` | Poison resistance |
| `ActorValue::kPoisonResist::Base` | Base poison resistance |
| `ActorValue::kPoisonResist::Permanent` | Permanent poison resistance |
| `ActorValue::kPoisonResist::Clamped` | Clamped poison resistance |

---

## Combat Skills

| Field | Description |
|-------|-------------|
| `ActorValue::kOneHanded` | One-Handed skill level |
| `ActorValue::kOneHanded::Base` | Base One-Handed skill level |
| `ActorValue::kOneHanded::Permanent` | Permanent One-Handed skill level |
| `ActorValue::kOneHanded::Clamped` | Clamped One-Handed skill level |
| `ActorValue::kTwoHanded` | Two-Handed skill level |
| `ActorValue::kTwoHanded::Base` | Base Two-Handed skill level |
| `ActorValue::kTwoHanded::Permanent` | Permanent Two-Handed skill level |
| `ActorValue::kTwoHanded::Clamped` | Clamped Two-Handed skill level |
| `ActorValue::kArchery` | Archery skill level |
| `ActorValue::kArchery::Base` | Base Archery skill level |
| `ActorValue::kArchery::Permanent` | Permanent Archery skill level |
| `ActorValue::kArchery::Clamped` | Clamped Archery skill level |
| `ActorValue::kBlock` | Block skill level |
| `ActorValue::kBlock::Base` | Base Block skill level |
| `ActorValue::kBlock::Permanent` | Permanent Block skill level |
| `ActorValue::kBlock::Clamped` | Clamped Block skill level |

---

## Crafting Skills

| Field | Description |
|-------|-------------|
| `ActorValue::kSmithing` | Smithing skill level |
| `ActorValue::kSmithing::Base` | Base Smithing skill level |
| `ActorValue::kSmithing::Permanent` | Permanent Smithing skill level |
| `ActorValue::kSmithing::Clamped` | Clamped Smithing skill level |
| `ActorValue::kAlchemy` | Alchemy skill level |
| `ActorValue::kAlchemy::Base` | Base Alchemy skill level |
| `ActorValue::kAlchemy::Permanent` | Permanent Alchemy skill level |
| `ActorValue::kAlchemy::Clamped` | Clamped Alchemy skill level |
| `ActorValue::kEnchanting` | Enchanting skill level |
| `ActorValue::kEnchanting::Base` | Base Enchanting skill level |
| `ActorValue::kEnchanting::Permanent` | Permanent Enchanting skill level |
| `ActorValue::kEnchanting::Clamped` | Clamped Enchanting skill level |

---

## Armor Skills

| Field | Description |
|-------|-------------|
| `ActorValue::kHeavyArmor` | Heavy Armor skill level |
| `ActorValue::kHeavyArmor::Base` | Base Heavy Armor skill level |
| `ActorValue::kHeavyArmor::Permanent` | Permanent Heavy Armor skill level |
| `ActorValue::kHeavyArmor::Clamped` | Clamped Heavy Armor skill level |
| `ActorValue::kLightArmor` | Light Armor skill level |
| `ActorValue::kLightArmor::Base` | Base Light Armor skill level |
| `ActorValue::kLightArmor::Permanent` | Permanent Light Armor skill level |
| `ActorValue::kLightArmor::Clamped` | Clamped Light Armor skill level |

---

## Stealth Skills

| Field | Description |
|-------|-------------|
| `ActorValue::kPickpocket` | Pickpocket skill level |
| `ActorValue::kPickpocket::Base` | Base Pickpocket skill level |
| `ActorValue::kPickpocket::Permanent` | Permanent Pickpocket skill level |
| `ActorValue::kPickpocket::Clamped` | Clamped Pickpocket skill level |
| `ActorValue::kLockpicking` | Lockpicking skill level |
| `ActorValue::kLockpicking::Base` | Base Lockpicking skill level |
| `ActorValue::kLockpicking::Permanent` | Permanent Lockpicking skill level |
| `ActorValue::kLockpicking::Clamped` | Clamped Lockpicking skill level |
| `ActorValue::kSneak` | Sneak skill level |
| `ActorValue::kSneak::Base` | Base Sneak skill level |
| `ActorValue::kSneak::Permanent` | Permanent Sneak skill level |
| `ActorValue::kSneak::Clamped` | Clamped Sneak skill level |
| `ActorValue::kSpeech` | Speech skill level |
| `ActorValue::kSpeech::Base` | Base Speech skill level |
| `ActorValue::kSpeech::Permanent` | Permanent Speech skill level |
| `ActorValue::kSpeech::Clamped` | Clamped Speech skill level |

---

## Magic Skills

| Field | Description |
|-------|-------------|
| `ActorValue::kAlteration` | Alteration skill level |
| `ActorValue::kAlteration::Base` | Base Alteration skill level |
| `ActorValue::kAlteration::Permanent` | Permanent Alteration skill level |
| `ActorValue::kAlteration::Clamped` | Clamped Alteration skill level |
| `ActorValue::kConjuration` | Conjuration skill level |
| `ActorValue::kConjuration::Base` | Base Conjuration skill level |
| `ActorValue::kConjuration::Permanent` | Permanent Conjuration skill level |
| `ActorValue::kConjuration::Clamped` | Clamped Conjuration skill level |
| `ActorValue::kDestruction` | Destruction skill level |
| `ActorValue::kDestruction::Base` | Base Destruction skill level |
| `ActorValue::kDestruction::Permanent` | Permanent Destruction skill level |
| `ActorValue::kDestruction::Clamped` | Clamped Destruction skill level |
| `ActorValue::kIllusion` | Illusion skill level |
| `ActorValue::kIllusion::Base` | Base Illusion skill level |
| `ActorValue::kIllusion::Permanent` | Permanent Illusion skill level |
| `ActorValue::kIllusion::Clamped` | Clamped Illusion skill level |
| `ActorValue::kRestoration` | Restoration skill level |
| `ActorValue::kRestoration::Base` | Base Restoration skill level |
| `ActorValue::kRestoration::Permanent` | Permanent Restoration skill level |
| `ActorValue::kRestoration::Clamped` | Clamped Restoration skill level |

---

## Dragon-Related

| Field | Description |
|-------|-------------|
| `ActorValue::kDragonSouls` | Collected dragon souls |
| `ActorValue::kDragonSouls::Base` | Base dragon souls |
| `ActorValue::kDragonSouls::Permanent` | Permanent dragon souls |
| `ActorValue::kDragonSouls::Clamped` | Clamped dragon souls |
| `ActorValue::kShoutRecoveryMult` | Shout recovery multiplier |
| `ActorValue::kShoutRecoveryMult::Base` | Base shout recovery multiplier |
| `ActorValue::kShoutRecoveryMult::Permanent` | Permanent shout recovery multiplier |
| `ActorValue::kShoutRecoveryMult::Clamped` | Clamped shout recovery multiplier |
