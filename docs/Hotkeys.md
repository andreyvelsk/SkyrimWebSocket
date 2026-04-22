# Hotkeys Fields Reference

Hotkey fields return information about the player's eight quick-equip slots (the 1–8 keyboard number keys in Skyrim). Use `subscribe` or `query` to read them.

## Available Hotkey Fields

| Registry key | Value type | Description |
|---|---|---|
| `Hotkey::Items` | `array` | All 8 hotkey slots with their current bindings (spells, shouts, powers, or inventory items) |

---

## `Hotkey::Items` Element Shape

The array always contains exactly 8 entries, one per slot, in ascending slot order. Each entry always has `slot` and `bound`. When `bound` is `true`, the entry also carries `kind` and the fields appropriate for that kind.

### Unbound slot

```jsonc
{ "slot": 3, "bound": false }
```

### Spell / shout / power slot (`kind = "spell"`)

```jsonc
{
  "slot": 1,
  "bound": true,
  "kind": "spell",
  "name": "Flames",
  "formId": "0x0002DD29",
  "spellType": "Spell",
  "school": "Destruction",
  "cost": 14,
  "level": 0,
  "chargeTime": 0.0
}
```

### Inventory item slot (`kind = "item"`)

```jsonc
{
  "slot": 2,
  "bound": true,
  "kind": "item",
  "name": "Iron Sword",
  "formId": "0x00012EB7",
  "categoryType": "Weapon",
  "count": 1,
  "weight": 9.0,
  "value": 25,
  "isFavorite": true
}
```

---

## Field Reference

### Common fields (all entries)

| Field | Type | Description |
|---|---|---|
| `slot` | integer | Slot number in the range `1..8`. |
| `bound` | bool | `true` when a spell or item is bound to this slot. |

### Fields present when `bound = true`

| Field | Type | Description |
|---|---|---|
| `kind` | string | `"spell"` (includes shouts and powers) or `"item"`. |
| `name` | string | Display name of the bound form. |
| `formId` | string | Hex form ID, e.g. `"0x00012EB7"`. |

### Additional fields when `kind = "spell"`

| Field | Type | Description |
|---|---|---|
| `spellType` | string | `"Spell"`, `"Power"`, `"LesserPower"`, `"VoicePower"`, or `"Shout"`. |
| `school` | string | `"Destruction"`, `"Alteration"`, `"Conjuration"`, `"Illusion"`, `"Restoration"`, or `"None"`. |
| `cost` | integer | Real in-game magicka cost with player skill/perk modifiers applied. |
| `level` | integer | Minimum school skill required: `0`=Novice, `25`=Apprentice, `50`=Adept, `75`=Expert, `100`=Master. |
| `chargeTime` | float | Charge time in seconds before the spell fires. |

### Additional fields when `kind = "item"`

| Field | Type | Description |
|---|---|---|
| `categoryType` | string | `"Weapon"`, `"Apparel"`, `"Book"`, `"Potion"`, `"Food"`, `"Ingredient"`, `"Misc"`, `"Ammo"`, `"Key"`, `"SoulGem"`, `"Scroll"`, or `"Unknown"`. |
| `count` | integer | Stack count in the player's inventory. |
| `weight` | float | Total weight of the stack. |
| `value` | integer | Base value in gold. |
| `isFavorite` | bool | Whether the item is marked as a favourite. |

---

## Example

```json
{
  "type": "query",
  "id": "hotkeys-q",
  "fields": {
    "hotkeys": "Hotkey::Items"
  }
}
```

**Server reply:**

```jsonc
{
  "type": "data",
  "id": "hotkeys-q",
  "ts": 1712462400123,
  "fields": {
    "hotkeys": [
      { "slot": 1, "bound": true, "kind": "spell", "name": "Flames", "formId": "0x0002DD29", "spellType": "Spell", "school": "Destruction", "cost": 14, "level": 0, "chargeTime": 0.0 },
      { "slot": 2, "bound": true, "kind": "item",  "name": "Iron Sword", "formId": "0x00012EB7", "categoryType": "Weapon", "count": 1, "weight": 9.0, "value": 25, "isFavorite": true },
      { "slot": 3, "bound": false },
      { "slot": 4, "bound": false },
      { "slot": 5, "bound": false },
      { "slot": 6, "bound": false },
      { "slot": 7, "bound": false },
      { "slot": 8, "bound": false }
    ]
  }
}
```
