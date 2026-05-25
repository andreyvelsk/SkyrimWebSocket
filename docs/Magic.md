# Magic Fields Reference

Magic fields return information about spells known and equipped by the player. All magic fields return JSON arrays with detailed spell and school information.

## Available Magic Fields

| Registry key | Value type | Description |
|---|---|---|
| `Magic::Categories` | `array` | Non-empty magic schools with spell counts (only includes schools where player knows at least one spell) |
| `Magic::Items::Destruction` | `array` | Destruction spells known by player |
| `Magic::Items::Alteration` | `array` | Alteration spells known by player |
| `Magic::Items::Conjuration` | `array` | Conjuration spells known by player |
| `Magic::Items::Illusion` | `array` | Illusion spells known by player |
| `Magic::Items::Restoration` | `array` | Restoration spells known by player |
| `Magic::Items::Enchanting` | `array` | Enchanting spells known by player |
| `Magic::Items::Shouts` | `array` | Dragon shouts known by player |
| `Magic::Items::Powers` | `array` | Greater powers known by player |
| `Magic::Items::LesserPowers` | `array` | Lesser powers known by player |

---

## \`Magic::Categories\` Element Shape

\`\`\`jsonc
{
  "categoryId": "Destruction",
  "name": "Destruction",
  "count": 5
}
\`\`\`

- \`categoryId\` — stable internal identifier, always English (e.g. \`"Destruction"\`, \`"Alteration"\`, etc.)
- \`name\` — in-game localized display name when available via GMST, otherwise equals \`categoryId\`
- \`count\` — number of spells known in this school (only includes regular castable spells, excludes powers, abilities, etc.)

### Possible \`categoryId\` values

- \`Destruction\` — Destruction spells
- \`Alteration\` — Alteration spells
- \`Conjuration\` — Conjuration spells
- \`Illusion\` — Illusion spells
- \`Restoration\` — Restoration spells
- \`Enchanting\` — Enchanting spells

> **Note:** Only non-empty schools are returned (player must know at least one spell in the school).

---

## Spell Entry Shape

All per-school and \`Magic::Items::*\` arrays return spell entries with the following structure:

\`\`\`jsonc
{
  "name": "Fireball",
  "formId": "0x0000A23E",
  "categoryType": "Destruction",
  "cost": 152,
  "costValue": 213,
  "level": 50,
  "castingType": "FireAndForget",
  "delivery": "Aimed",
  "range": 300,
  "chargeTime": 0.7,
  "effects": [
    {
      "name": "Fire Damage",
      "magnitude": 40.0,
      "duration": 0,
      "descriptionTemplate": "<mag> points of Fire damage",
      "description": "40 points of Fire damage"
    }
  ],
  "isEquipped": true,
  "equippedHand": "right",
  "isActive": false,
  "isFavorite": true,
  "hotkeys": [0, 3]
}
\`\`\`

### Spell Field Descriptions

- \`name\` — Spell display name
- \`formId\` — Unique spell identifier (hex string format: \`"0xHHHHHHHH"\`)
- \`categoryType\` — Magic school: \`"Destruction"\`, \`"Alteration"\`, \`"Conjuration"\`, \`"Illusion"\`, \`"Restoration"\`, or \`"Enchanting"\`
- \`cost\` — **Effective magicka cost** with player skill and perk modifiers applied. This is the actual cost the player will pay when casting.
- \`costValue\` — **Base cost** (raw, unmodified). Either the explicit \`costOverride\` if set, or the base calculated cost with no actor modifiers.
- \`level\` — **Minimum required school skill** to cast (Novice=0, Apprentice=25, Adept=50, Expert=75, Master=100)
- \`castingType\` — How the spell is cast: \`"ConstantEffect"\`, \`"FireAndForget"\`, \`"Concentration"\`, or \`"Scroll"\`
- \`delivery\` — Delivery method: \`"Self"\`, \`"Touch"\`, \`"Aimed"\`, \`"TargetActor"\`, or \`"TargetLocation"\`
- \`range\` — Spell range in units (0 for self/touch, varies for ranged)
- \`chargeTime\` — Charge time in seconds (0.0 for instant/fire-and-forget)
- \`effects\` — Array of effect objects (see [Effect Object](#effect-object) below)
- \`isEquipped\` — \`true\` if the spell is currently equipped for casting in either hand
- \`equippedHand\` — Where the spell is equipped: \`"right"\`, \`"left"\`, \`"both"\` (dual-cast), or \`null\` if not equipped
- \`isActive\` — \`true\` if currently being cast by the player
- \`isFavorite\` — \`true\` if this spell is marked as a favorite (appears in spell menu favorites list)
- \`hotkeys\` — Array of hotkey slot numbers (0-7) where this spell is assigned (empty if not hotkeyed)

---

## Effect Object

Each spell effect includes:

\`\`\`jsonc
{
  "name": "Fire Damage",
  "magnitude": 40.0,
  "duration": 0,
  "descriptionTemplate": "<mag> points of Fire damage",
  "description": "40 points of Fire damage"
}
\`\`\`

- \`name\` — Effect display name (e.g. "Fire Damage", "Paralyze")
- \`magnitude\` — Effect magnitude/intensity (formatted as integer when whole number, otherwise one decimal place)
- \`duration\` — Effect duration in seconds (0 = instant)
- \`descriptionTemplate\` — Template text with placeholders like \`<mag>\` and \`<dur>\` (raw from GMST)
- \`description\` — Resolved description with template values substituted (e.g. \`"40 points of Fire damage"\`)

---

## Examples

### Example 1 — Query spell categories

Get a list of which magic schools the player knows spells in, with counts.

**Client sends:**
\`\`\`json
{
  "type": "query",
  "id": "magic-schools",
  "fields": {
    "schools": "Magic::Categories"
  }
}
\`\`\`

**Server replies:**
\`\`\`json
{
  "type": "data",
  "id": "magic-schools",
  "ts": 1712462400123,
  "fields": {
    "schools": [
      { "categoryId": "Destruction", "name": "Destruction", "count": 12 },
      { "categoryId": "Restoration", "name": "Restoration", "count": 8 },
      { "categoryId": "Alteration", "name": "Alteration", "count": 5 }
    ]
  }
}
\`\`\`

---

## Spell Scope

Spells returned include:

1. **Base NPC spells** — Spells baked into the player's character at creation
2. **Learned spells** — Spells acquired at runtime via:
   - Spell tomes
   - Console commands
   - Mods/quests
   - Any other method that adds to the player's \`addedSpells\` list

Note: The list does **not** include powers, abilities, diseases, or other non-castable magic items. Only regular spells (SpellType::kSpell) are included.

---

## Shout Entry Shape

`Magic::Items::Shouts` returns an array of shout entries:

```jsonc
{
  "name": "Unrelenting Force",
  "formId": "0x00013E08",
  "description": "Your thu'um unbalances the very force of gravity, sending your hapless target flying.",
  "words": [
    {
      "name": "FUS",
      "formId": "0x0001362A",
      "recoveryTime": 15.0,
      "isKnown": true
    },
    {
      "name": "RO",
      "formId": "0x00017E22",
      "recoveryTime": 20.0,
      "isKnown": true
    },
    {
      "name": "DAH",
      "formId": "0x00017E23",
      "recoveryTime": 45.0,
      "isKnown": false
    }
  ],
  "isEquipped": true,
  "isFavorite": true,
  "hotkeys": [0]
}
```

### Shout Field Descriptions

- `name` — Shout display name (e.g. `"Unrelenting Force"`)
- `formId` — Unique shout identifier (hex string format: `"0xHHHHHHHH"`)
- `description` — Localized in-game description of the shout (empty string if none)
- `words` — Array of word-of-power entries (up to 3; only includes words with valid data):
  - `name` — Word display name (e.g. `"FUS"`)
  - `formId` — Word form identifier
  - `recoveryTime` — Voice cooldown in seconds when this many words are used
  - `isKnown` — `true` if the player has unlocked this word of power
- `isEquipped` — `true` if this shout is the currently active voice power
- `isFavorite` — `true` if this shout is in the magic favorites list
- `hotkeys` — Array of hotkey slot numbers (0-7) where this shout is assigned (empty if not hotkeyed)

---

## Power Entry Shape

`Magic::Items::Powers` and `Magic::Items::LesserPowers` return arrays of power entries:

```jsonc
{
  "name": "Breton Heritage",
  "formId": "0x00017102",
  "spellType": "Power",
  "cost": 0,
  "effects": [
    {
      "name": "Dragonskin",
      "magnitude": 50.0,
      "duration": 60,
      "descriptionTemplate": "Absorb <mag>% of spell damage for <dur> seconds.",
      "description": "Absorb 50% of spell damage for 60 seconds."
    }
  ],
  "isEquipped": false,
  "isFavorite": false,
  "hotkeys": []
}
```

### Power Field Descriptions

- `name` — Power display name
- `formId` — Unique power identifier (hex string format: `"0xHHHHHHHH"`)
- `spellType` — `"Power"` (greater power, once-per-day) or `"LesserPower"` (unlimited use)
- `cost` — Magicka cost (typically `0` for powers)
- `effects` — Array of effect objects (same shape as [Effect Object](#effect-object))
- `isEquipped` — `true` if this power is the currently active voice power
- `isFavorite` — `true` if this power is in the magic favorites list
- `hotkeys` — Array of hotkey slot numbers (0-7) where this power is assigned (empty if not hotkeyed)

---

## Game Commands for Spells

The following commands are available for spells (see [PROTOCOL.md](../PROTOCOL.md) for full command documentation):

| Command | Description |
|---|---|
| `equip_spell` | Equips a known spell to a hand slot for casting |
| `unequip_spell` | Unequips a spell from a hand slot |
| `favorite_spell` | Toggles favorite status on a spell, power, or lesser power |
| `equip_shout` | Equips a known dragon shout to the voice slot |
| `unequip_shout` | Removes a dragon shout from the voice slot |
| `equip_power` | Equips a known power or lesser power to the voice slot |
| `favorite_shout` | Toggles favorite status on a known dragon shout |

---

## Master-Level Dual-Cast Behavior

Master-level spells (those requiring 100 skill in their school) are inherently dual-cast weapons — they occupy both hands and cannot be single-handed.

- When a master-level spell is equipped in one hand, it is automatically equipped in both
- Unequipping a master-level spell from either hand removes it from both hands completely
- Master-level spells will appear in \`equippedHand: "both"\` if equipped
