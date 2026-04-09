# ActorValue Fields Reference

All ActorValue fields return `float` values.

## Available ActorValue Fields

| Registry key | Description |
|---|---|
| `ActorValue::kHealth` | Current health points |
| `ActorValue::kMagicka` | Current magicka points |
| `ActorValue::kStamina` | Current stamina points |
| `ActorValue::kHealRate` | Health regeneration rate |
| `ActorValue::kMagickaRate` | Magicka regeneration rate |
| `ActorValue::kStaminaRate` | Stamina regeneration rate |
| `ActorValue::kHealRateMult` | Health regeneration multiplier |
| `ActorValue::kMagickaRateMult` | Magicka regeneration multiplier |
| `ActorValue::kStaminaRateMult` | Stamina regeneration multiplier |
| `ActorValue::kSpeedMult` | Movement speed multiplier |
| `ActorValue::kCarryWeight` | Carry weight capacity |
| `ActorValue::kAttackDamageMult` | Attack damage multiplier |
| `ActorValue::kCriticalChance` | Critical hit chance |
| `ActorValue::kDamageResist` | Physical damage resistance |
| `ActorValue::kResistMagic` | Magic resistance |
| `ActorValue::kResistFire` | Fire resistance |
| `ActorValue::kResistFrost` | Frost resistance |
| `ActorValue::kResistShock` | Shock resistance |
| `ActorValue::kPoisonResist` | Poison resistance |
| `ActorValue::kOneHanded` | One-Handed skill level |
| `ActorValue::kTwoHanded` | Two-Handed skill level |
| `ActorValue::kArchery` | Archery skill level |
| `ActorValue::kBlock` | Block skill level |
| `ActorValue::kSmithing` | Smithing skill level |
| `ActorValue::kAlchemy` | Alchemy skill level |
| `ActorValue::kEnchanting` | Enchanting skill level |
| `ActorValue::kHeavyArmor` | Heavy Armor skill level |
| `ActorValue::kLightArmor` | Light Armor skill level |
| `ActorValue::kPickpocket` | Pickpocket skill level |
| `ActorValue::kLockpicking` | Lockpicking skill level |
| `ActorValue::kSneak` | Sneak skill level |
| `ActorValue::kSpeech` | Speech skill level |
| `ActorValue::kAlteration` | Alteration skill level |
| `ActorValue::kConjuration` | Conjuration skill level |
| `ActorValue::kDestruction` | Destruction skill level |
| `ActorValue::kIllusion` | Illusion skill level |
| `ActorValue::kRestoration` | Restoration skill level |
| `ActorValue::kDragonSouls` | Collected dragon souls |
| `ActorValue::kShoutRecoveryMult` | Shout recovery multiplier |

## Value Type Modifiers

Each `ActorValue` key also has `::Base`, `::Permanent`, and `::Clamped` variants.

For example:
- `ActorValue::kHealth` — Current value with temporary modifiers
- `ActorValue::kHealth::Base` — Base value without modifications
- `ActorValue::kHealth::Permanent` — Permanent base value
- `ActorValue::kHealth::Clamped` — Value clamped to valid min/max ranges

This applies to all ActorValue fields listed above.
