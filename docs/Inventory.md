# Inventory Fields Reference

Inventory fields return complex JSON arrays or objects.

## Available Inventory Fields

| Registry key | Value type | Description |
|---|---|---|
| `Inventory::Categories` | `array` | Non-empty inventory categories with total item counts (food is its own "Food" entry, gold excluded from Misc; includes a Favorites entry when applicable) |
| `Inventory::Gold` | `integer` | Player's current gold amount |
| `Inventory::Items::Weapons` | `array` | Weapons in player inventory |
| `Inventory::Items::Apparel` | `array` | Apparel in player inventory |
| `Inventory::Items::Books` | `array` | Books in player inventory |
| `Inventory::Items::Potions` | `array` | Potions in player inventory (food excluded) |
| `Inventory::Items::Food` | `array` | Food items in player inventory |
| `Inventory::Items::Ingredients` | `array` | Ingredients in player inventory |
| `Inventory::Items::Misc` | `array` | Miscellaneous items in player inventory (gold excluded) |
| `Inventory::Items::Ammo` | `array` | Ammunition in player inventory |
| `Inventory::Items::Keys` | `array` | Keys in player inventory |
| `Inventory::Items::SoulGems` | `array` | Soul gems in player inventory |
| `Inventory::Items::Scrolls` | `array` | Scrolls in player inventory |
| `Inventory::Items::Favorites` | `array` | All favorited items across every category |

---

## `Inventory::Categories` Element Shape

```jsonc
{ "categoryId": "Weapons", "name": "Weapons", "count": 5 }
```

- `categoryId` — stable internal identifier, always English (e.g. `"Weapons"`, `"Potions"`)
- `name` — in-game localized display name when available via GMST, otherwise equals `categoryId`

---

## Base Item Fields (All Categories)

Every `Inventory::Items::*` category element has these fields:

```jsonc
{ 
  "name": "Iron Sword", 
  "formId": "0x00012EB7", 
  "count": 1, 
  "weight": 9.0, 
  "value": 25, 
  "isFavorite": false, 
  "isStolen": false 
}
```

- `name` — Item display name
- `formId` — Unique form identifier (hex string)
- `count` — Quantity in inventory
- `weight` — Weight per item
- `value` — Gold value per item
- `isFavorite` — Is this item in favorites
- `isStolen` — `true` when the item stack carries a stolen (red-hand) flag

---

## Category-Specific Extended Fields

### Weapons

Additional fields:
- `isEquipped` (bool) — Currently equipped on character
- `weaponType` (string) — Weapon category: `"OneHandSword"`, `"OneHandDagger"`, `"OneHandAxe"`, `"OneHandMace"`, `"TwoHandSword"`, `"TwoHandAxe"`, `"Bow"`, `"Staff"`, `"Crossbow"`, `"HandToHand"`, or `null`
- `isTwoHanded` (bool) — `true` for two-handed melee weapons, bows, and crossbows (occupies both hand slots)
- `equipSlots` (array of strings) — Valid equip targets: `["right", "left"]` for one-handed, `["right"]` for two-handed
- `equippedHand` (string or null) — Current equip hand: `"right"`, `"left"`, `"both"` (same weapon dual-wielded), or `null` if not equipped
- `baseDamage` (float) — Weapon base damage before perks
- `damage` (float) — Effective damage = `baseDamage × kAttackDamageMult`
- `enchantment` (object or null) — See [Enchantment Object](#enchantment-object)
- `enchantmentCharge` (number or null) — Current enchantment charge if applicable

### Apparel

Additional fields:
- `isEquipped` (bool) — Currently equipped on character
- `armorTypeId` (string) — Stable key: `"Heavy"`, `"Light"`, or `"Clothing"`
- `armorType` (string) — Localized in-game display name via GMST
- `baseArmorRating` (float) — Raw form value before perks
- `armorRating` (float) — Effective value as shown in inventory = `baseArmorRating × (1 + kArmorPerks/100)`
- `bodySlots` (array of strings) — e.g. `["Body", "Forearms"]`
- `enchantment` (object or null) — See [Enchantment Object](#enchantment-object)

### Potions

Additional fields:
- `effects` (array) — See [Effects Array](#effects-array-element)

### Food

Additional fields:
- `effects` (array) — See [Effects Array](#effects-array-element)

### Ingredients

Additional fields:
- `effects` (array of objects) — Each has `{ "name": string, "known": bool }` (up to 4 entries)

### Books

Additional fields:
- `description` (string) — From game's TESDescription

### Scrolls

Additional fields:
- `effects` (array) — See [Effects Array](#effects-array-element)

### Soul Gems

Additional fields:
- `capacity` (string) — Max soul level: `"Petty"`, `"Lesser"`, `"Common"`, `"Greater"`, `"Grand"`, or `"None"`
- `containedSoul` (string) — Current fill level, same values as capacity

### Favorites

Additional fields:
- `isEquipped` (bool) — Currently equipped on character
- `type` (string) — Original category ID (e.g. `"Weapons"`, `"Apparel"`)

---

## Enchantment Object

Used by Weapons and Apparel. `null` when no enchantment is present.

```jsonc
{
  "name": "Fiery Soul Trap",
  "effects": [
    {
      "name": "Fire Damage",
      "magnitude": 10.0,
      "duration": 1,
      "descriptionTemplate": "Do <mag> points of fire damage.",
      "description": "Do 10 points of fire damage."
    },
    {
      "name": "Soul Trap",
      "magnitude": 0.0,
      "duration": 5,
      "descriptionTemplate": "If target dies within <dur> seconds, fills a soul gem.",
      "description": "If target dies within 5 seconds, fills a soul gem."
    }
  ]
}
```

- `name` — Localized enchantment name
- `effects` — Array of effect objects (see [Effects Array](#effects-array-element))

---

## Effects Array Element

Used by Potions, Food, Scrolls, and within Enchantment objects.

```jsonc
{
  "name": "Restore Health",
  "magnitude": 50.0,
  "duration": 0,
  "descriptionTemplate": "Restore <mag> points of Health.",
  "description": "Restore 50 points of Health."
}
```

- `name` — Localized effect name from the game
- `magnitude` — Effect magnitude/strength
- `duration` — Effect duration in seconds (0 = instant)
- `descriptionTemplate` — Localized description template from the EffectSetting DNAM field
  - May contain placeholders: `<mag>` (magnitude), `<dur>` (duration)
- `description` — Ready-to-display string with placeholders replaced
  - Integers when no fractional part, otherwise one decimal place
