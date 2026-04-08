# Inventory Field Reference

This document describes all available Inventory query types and the structure of the returned data.

## Supported Inventory Types

Inventory fields follow the `Inventory::k*` naming convention. Each query returns detailed item information organized by type.

| Registry key | Description | Returns |
|---|---|---|
| `Inventory::kAll` | Complete player inventory grouped by type | Object with arrays for each item type |
| `Inventory::kWeapon` | All weapons in inventory | Array of weapon objects |
| `Inventory::kArmor` | All armor pieces in inventory | Array of armor objects |
| `Inventory::kAmmo` | All ammunition | Array of ammo objects |
| `Inventory::kPotion` | All potions and poisons | Array of potion objects |
| `Inventory::kFood` | All food and drinks | Array of food objects |
| `Inventory::kBook` | All books and spell tomes | Array of book objects |
| `Inventory::kIngredient` | All alchemy ingredients | Array of ingredient objects |
| `Inventory::kMisc` | All miscellaneous items | Array of misc objects |

---

## Common Item Fields

All item objects contain these base fields:

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Item name |
| `itemType` | string | Type of item: "weapon", "armor", "ammo", "potion", "food", "book", "ingredient", "misc", "spell" |
| `formId` | uint32 | Form ID of the item (unique identifier) |
| `count` | int | Quantity of this item in inventory |
| `weight` | float | Weight per single item |
| `value` | float | Gold value per single item |
| `isQuestItem` | bool | Indicates if this is a quest-critical item |
| `durability` | float or null | Item durability percentage (0-100), or -1 if not applicable |

---

## Weapon Object

Extends common item fields with weapon-specific properties.

**Example weapon object:**
```json
{
  "name": "Iron Sword",
  "itemType": "weapon",
  "formId": 12345,
  "count": 1,
  "weight": 12.0,
  "value": 50,
  "isQuestItem": false,
  "durability": 95.5,
  "weaponType": "oneHanded",
  "damage": 8.0,
  "attackSpeed": 1.0,
  "reach": 1.3,
  "enchantment": null
}
```

**Extended weapon fields:**

| Field | Type | Description |
|-------|------|-------------|
| `weaponType` | string | Weapon classification: "oneHanded", "twoHanded", "bow", "crossbow" |
| `damage` | float | Weapon damage value |
| `attackSpeed` | float | Attack speed modifier (1.0 = normal) |
| `reach` | float | Attack reach distance |
| `enchantment` | object or null | Enchantment data if present |
| `enchantment.name` | string | Enchantment name |
| `enchantment.cost` | uint32 | Magicka cost per use |
| `enchantment.charge` | float | Current charge percentage (0-100), -1 if not enchanted |

---

## Armor Object

Extends common item fields with armor-specific properties.

**Example armor object:**
```json
{
  "name": "Iron Helmet",
  "itemType": "armor",
  "formId": 67890,
  "count": 1,
  "weight": 3.0,
  "value": 15,
  "isQuestItem": false,
  "durability": 100.0,
  "armorType": "head",
  "armor": 15.0,
  "enchantment": null
}
```

**Extended armor fields:**

| Field | Type | Description |
|-------|------|-------------|
| `armorType` | string | Armor slot: "head", "chest", "gloves", "boots", "shield" |
| `armor` | float | Armor class/rating value |
| `enchantment` | object or null | Enchantment data if present |
| `enchantment.name` | string | Enchantment name |
| `enchantment.cost` | uint32 | Magicka cost per use |
| `enchantment.charge` | float | Current charge percentage (0-100), -1 if not enchanted |

---

## Potion/Poison Object

Extends common item fields with alchemical properties.

**Example potion object:**
```json
{
  "name": "Potion of Healing",
  "itemType": "potion",
  "formId": 24680,
  "count": 5,
  "weight": 0.5,
  "value": 25,
  "isQuestItem": false,
  "durability": -1,
  "isPoison": false,
  "effects": [
    {
      "name": "Restore Health",
      "magnitude": 75.0,
      "duration": 0,
      "area": 0
    }
  ]
}
```

**Extended potion fields:**

| Field | Type | Description |
|-------|------|-------------|
| `isPoison` | bool | True for poisons, false for potions/elixirs |
| `effects` | array | Array of active/inactive effects in this potion |
| `effects[].name` | string | Effect name |
| `effects[].magnitude` | float | Effect magnitude/strength |
| `effects[].duration` | uint32 | Duration in seconds (0 = instant) |
| `effects[].area` | uint32 | Area of effect radius (0 = personal) |

---

## Book Object

Extends common item fields with book-specific properties.

**Example book object:**
```json
{
  "name": "Destruction Skill Book",
  "itemType": "book",
  "formId": 13572,
  "count": 1,
  "weight": 1.0,
  "value": 15,
  "isQuestItem": false,
  "durability": -1,
  "isSkillBook": true,
  "skill": 18,
  "relatedQuest": null
}
```

**Extended book fields:**

| Field | Type | Description |
|-------|------|-------------|
| `isSkillBook` | bool | Indicates if reading increases a skill |
| `skill` | uint32 or null | ActorValue ID if this is a skill book (null otherwise) |
| `relatedQuest` | string or null | Quest name if this book is related to a quest |

---

## Ammunition Object

Extends common item fields with ammo-specific properties.

**Example ammo object:**
```json
{
  "name": "Iron Arrow",
  "itemType": "ammo",
  "formId": 20100,
  "count": 42,
  "weight": 0.1,
  "value": 1,
  "isQuestItem": false,
  "durability": -1,
  "damage": 8.0
}
```

**Extended ammo fields:**

| Field | Type | Description |
|-------|------|-------------|
| `damage` | float | Damage value per shot |

---

## Ingredient Object

Extends common item fields with alchemical effects.

**Example ingredient object:**
```json
{
  "name": "Imp Stool",
  "itemType": "ingredient",
  "formId": 50008,
  "count": 3,
  "weight": 0.1,
  "value": 3,
  "isQuestItem": false,
  "durability": -1,
  "effects": [
    {
      "name": "Damage Health"
    },
    {
      "name": "Paralysis"
    },
    {
      "name": "Ravage Magicka"
    },
    {
      "name": "Lingering Damage Health"
    }
  ]
}
```

**Extended ingredient fields:**

| Field | Type | Description |
|-------|------|-------------|
| `effects` | array | Array of alchemy effects present in ingredient |
| `effects[].name` | string | Effect name |

---

## Misc/Food Object

Extends common item fields only. No additional fields.

**Example misc object:**
```json
{
  "name": "Iron Ore",
  "itemType": "misc",
  "formId": 4000C,
  "count": 10,
  "weight": 1.0,
  "value": 5,
  "isQuestItem": false,
  "durability": -1
}
```

**Example food object:**
```json
{
  "name": "Bread",
  "itemType": "food",
  "formId": 50000,
  "count": 3,
  "weight": 1.0,
  "value": 1,
  "isQuestItem": false,
  "durability": -1
}
```

---

## Special: Inventory::kAll Response

When querying `Inventory::kAll`, the response is an object containing separate arrays for each item type:

```json
{
  "type": "data",
  "ts": 1712462400123,
  "fields": {
    "inventory": {
      "actor": "Prisoner",
      "type": "inventory",
      "weapons": [ ... ],
      "armor": [ ... ],
      "ammo": [ ... ],
      "potions": [ ... ],
      "food": [ ... ],
      "books": [ ... ],
      "ingredients": [ ... ],
      "misc": [ ... ],
      "spells": [ ... ]
    }
  }
}
```

The `spells` array contains known spells and abilities (not crafted items, but learned by the character).
