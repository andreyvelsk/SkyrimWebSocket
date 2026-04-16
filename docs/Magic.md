# Magic Fields Reference

Magic fields return complex JSON arrays describing the player's known spells, powers, abilities, and shouts.

## Available Magic Fields

| Registry key | Value type | Description |
|---|---|---|
| `Magic::Categories` | `array` | Non-empty magic schools that the player knows spells in (includes count per school) |
| `Magic::Items::PlayerSpells` | `array` | Player spells (obtained through training, loot, level-up) |
| `Magic::Items::LesserPowers` | `array` | Lesser powers (typically 1 use per day) |
| `Magic::Items::Powers` | `array` | Powers and abilities (passive effects, unlimited uses) |
| `Magic::Items::Shouts` | `array` | Shouts and dragon powers |
| `Magic::Items::Diseases` | `array` | Disease and poison effects active on the player |

---

## `Magic::Categories` Element Shape

```jsonc
{ "categoryId": "Destruction", "name": "Destruction", "count": 12 }

- `categoryId` — stable internal identifier, always English (e.g. `"Destruction"`, `"Alteration"`)
- `name` — in-game localized display name via GMST, otherwise equals `categoryId`
- `count` — number of spells known in this school
```

### Possible `categoryId` values

Below are all possible values for the `categoryId` field that `Magic::Categories` may return (stable internal keys):

- `Alteration` — Alteration (RE::ActorValue::kAlteration)
- `Conjuration` — Conjuration (RE::ActorValue::kConjuration)
- `Destruction` — Destruction (RE::ActorValue::kDestruction)
- `Illusion` — Illusion (RE::ActorValue::kIllusion)
- `Restoration` — Restoration (RE::ActorValue::kRestoration)

Note: only schools with at least one known spell are returned; order is not guaranteed.

---

## Spell/Power/Ability Element Shape

Common shape for `Magic::Items::PlayerSpells`, `Magic::Items::LesserPowers`, etc.:

```jsonc
{
  "name": "Fireball",
  "formId": "0x00012ABD",
  "categoryId": "Destruction",
  "categoryName": "Destruction",
  "cost": 165,
  "level": "Adept",
  "isEquipped": true,
  "isTwoHanded": true,
  "equippedHand": "right",
  "effects": [
    {
      "name": "Fire Damage",
      "magnitude": 50.0,
      "duration": 0,
      "descriptionTemplate": "Do <mag> points of fire damage.",
      "description": "Do 50 points of fire damage."
    }
  ]
}
```

### Common Fields (all types)

- `name` — Spell display name
- `formId` — Unique form identifier (hex string)
- `categoryId` — Magic school identifier (stable English key)
- `categoryName` — Localized magic school name (e.g. "Destruction", "Восстановление")
- `cost` — Magicka cost to cast (for spells) or activation cost
- `level` — Required spell level. Possible values: `"Novice"`, `"Apprentice"`, `"Adept"`, `"Expert"`, `"Master"`
- `effects` — Array of effect objects (see [Effects Array](#effects-array))

### Player Spells & Equipped Spells

`Magic::Items::PlayerSpells` items include equip state:

- `isEquipped` (bool) — Currently equipped in a hand for quick casting
- `isTwoHanded` (bool) — Always `true` for player spells (occupy both hands conceptually)
- `equippedHand` (string or null) — Equipped casting source: `"right"`, `"left"`, `"both"`, or `null` if not equipped

### Lesser Powers

`Magic::Items::LesserPowers` includes:

- `usesPerDay` (integer) — Always `1` (recharges daily)
- `isEquipped` (bool) — Always `false` (cannot quick-equip lesser powers)
- `isTwoHanded` (bool) — Always `false`
- `equippedHand` — Always `null`

### Powers & Abilities

`Magic::Items::Powers` includes:

- `usesPerDay` (integer) — Always `0` (unlimited uses)
- `isEquipped` (bool) — Always `false` (cannot quick-equip)
- `isTwoHanded` (bool) — Always `false`
- `equippedHand` — Always `null`

### Shouts

`Magic::Items::Shouts` includes:

- `usesPerDay` (integer) — Always `0` (recharge after combat)
- `isEquipped` (bool) — Always `false` (cannot quick-equip)
- `isTwoHanded` (bool) — Always `false`
- `equippedHand` — Always `null`

### Diseases

`Magic::Items::Diseases` includes:

- `isEquipped` (bool) — Always `false` (passive effects)
- `isTwoHanded` (bool) — Always `false`
- `equippedHand` — Always `null`

---

## Effects Array

Used by all magic items (spells, powers, shouts, diseases). Same format as [Inventory Effects](Inventory.md#effects-array-element).

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
- `descriptionTemplate` — Localized description template with placeholders (`<mag>`, `<dur>`)
- `description` — Ready-to-display string with placeholders replaced
  - Integers when no fractional part, otherwise one decimal place

---

## Magic Commands

### Equip Spell

Equip a player spell to a casting hand for quick casting.

```jsonc
{
  "type": "command",
  "id": "cmd-1",
  "command": "equipSpell",
  "formId": "0x00012ABD",
  "hand": "right"  // "right" or "left" (default "right")
}
```

**Response:**
```jsonc
{
  "type": "commandResult",
  "id": "cmd-1",
  "success": true
}
```

**Error cases:**
- Spell not found
- Spell not in player's known spells
- Invalid formId

### Unequip Spell

Unequip a spell from a casting hand.

```jsonc
{
  "type": "command",
  "id": "cmd-2",
  "command": "unequipSpell",
  "formId": "0x00012ABD",
  "hand": "right"  // "right", "left", or "both"/"all"
}
```

**Response:**
```jsonc
{
  "type": "commandResult",
  "id": "cmd-2",
  "success": true
}
```

**Parameters:**
- `hand` — Which hand(s) to unequip from:
  - `"right"` — unequip from right hand only
  - `"left"` — unequip from left hand only
  - `"both"` or `"all"` — unequip from all hands

---

## Notes

- Only player-known spells/powers/abilities are returned.
- Lesser powers, powers, shouts, and diseases cannot be quick-equipped and return `isEquipped: false`.
- Spells occupy both hands conceptually (`isTwoHanded: true`), though the engine tracks them separately per hand.
- Effect descriptions use the same placeholder substitution system as [Inventory](Inventory.md#effects-array-element).
- Magic school names are localized automatically via GMST lookups (e.g. "Destruction" → "Destruction" (EN) or "Разрушение" (RU)).
