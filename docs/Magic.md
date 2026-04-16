# Magic Fields Reference

Magic fields return complex JSON arrays or objects for spells and magic schools.

## Available Magic Fields

| Registry key | Value type | Description |
|---|---|---|
| `Magic::Categories` | `array` | Non-empty magic school categories with total spell counts |
| `Magic::Items::Alteration` | `array` | Alteration spells known by player |
| `Magic::Items::Conjuration` | `array` | Conjuration spells known by player |
| `Magic::Items::Destruction` | `array` | Destruction spells known by player |
| `Magic::Items::Illusion` | `array` | Illusion spells known by player |
| `Magic::Items::Restoration` | `array` | Restoration spells known by player |

---

## `Magic::Categories` Element Shape

```jsonc
{ "categoryId": "Destruction", "name": "Destruction", "count": 5 }

- `categoryId` — stable internal identifier, always English (e.g. `"Destruction"`, `"Alteration"`)
- `name` — in-game localized display name from GMST, otherwise equals `categoryId`

```

---

### Possible `categoryId` values

Below are all possible values for the `categoryId` field that `Magic::Categories` may return (stable internal keys):

- `Alteration` — Alteration school spells
- `Conjuration` — Conjuration school spells
- `Destruction` — Destruction school spells
- `Illusion` — Illusion school spells
- `Restoration` — Restoration school spells

- Note: only non-empty categories are returned; order is not guaranteed. The localized `name` is looked up via GMST and falls back to `categoryId` when absent.

---

## Base Spell Fields

All spells include these common fields:

```jsonc
{ 
  "name": "Fireball", 
  "formId": "0x0001C789", 
  "cost": 133, 
  "level": "Adept",
  "categoryType": "Destruction",
  "isTwoHanded": false,
  "equippedHand": "right",
  "isEquipped": true,
  "effects": [...]
}
```

- `name` — Spell display name
- `formId` — Unique form identifier (hex string)
- `cost` — Magicka cost to cast the spell
- `level` — Spell level: `"Novice"`, `"Apprentice"`, `"Adept"`, `"Expert"`, or `"Master"`
- `categoryType` — Magic school (see [Category Types](#category-types) below)
- `isTwoHanded` — `true` for spells that require both hands (rare in vanilla Skyrim)
- `equippedHand` — Current equip hand: `"right"`, `"left"`, `"both"`, or `null` if not equipped
- `isEquipped` — `true` when the spell is equipped in any hand
- `effects` — Array of spell effects (see [Effects Array](#effects-array-element))

---

## Category Types

Every spell includes a `categoryType` field that identifies its magic school. The possible values are:

- `Alteration` — Alteration school spells
- `Conjuration` — Conjuration school spells
- `Destruction` — Destruction school spells
- `Illusion` — Illusion school spells
- `Restoration` — Restoration school spells

---

## Effects Array Element

Used by all spells to describe their magical effects.

```jsonc
{
  "name": "Fire Damage",
  "magnitude": 40.0,
  "duration": 0,
  "descriptionTemplate": "Do <mag> points of fire damage.",
  "description": "Do 40 points of fire damage."
}
```

- `name` — Localized effect name from the game
- `magnitude` — Effect magnitude/strength
- `duration` — Effect duration in seconds (0 = instant)
- `descriptionTemplate` — Localized description template from the EffectSetting DNAM field
  - May contain placeholders: `<mag>` (magnitude), `<dur>` (duration)
- `description` — Ready-to-display string with placeholders replaced
  - Integers when no fractional part, otherwise one decimal place

---

## Commands

### Equip Spell

Equips a spell to the specified hand.

```jsonc
{
  "type": "command",
  "id": "cmd-1",
  "command": "equipSpell",
  "formId": "0x0001C789",
  "hand": "right"  // "right" or "left", default: "right"
}
```

### Unequip Spell

Unequips a spell from the specified hand.

```jsonc
{
  "type": "command",
  "id": "cmd-2",
  "command": "unequipSpell",
  "formId": "0x0001C789",
  "hand": "right"  // "right" or "left"
}
```

---

## Notes

- Spells must be known by the player before they can be equipped
- Most spells in Skyrim are one-handed; two-handed spells are rare
- Powers and passive abilities are not included in the spell lists (only castable spells)
- The `cost` field reflects the actual magicka cost after all modifiers (perks, enchantments, etc.)
- Spell effects use the same format as enchantment and potion effects for consistency
