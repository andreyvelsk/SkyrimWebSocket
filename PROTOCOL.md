# WebSocket Protocol — SkyrimWebSocket Plugin

The plugin starts a WebSocket server. By default it binds to `ws://127.0.0.1:8765`.
The address and port can be changed via an INI configuration file (see the [Configuration](#configuration) section).

After the connection is established the client drives all behaviour: it declares
which fields it wants, how often they should be delivered, and can query data
on demand. The server never sends anything until the client sends a message first.

---

## Message format

All messages are UTF-8 JSON objects with a required `"type"` field.

---

## Client → Server messages

### `subscribe`

Starts (or replaces) a repeating push subscription. The server will send a
`"data"` message at the requested frequency for as long as the session is open
or until `"unsubscribe"` is received.

```jsonc
{
  "type": "subscribe",
  "settings": {
    "frequency": 200,       // push interval in milliseconds (minimum: 50, default: 500)
    "sendOnChange": false   // when true, only send a message if at least one value changed
  },
  "fields": {
    "<alias>": "<registry key>",  // user-defined alias → registry key
    ...
  }
}
```

| Field | Required | Default | Description |
|---|---|---|---|
| `settings.frequency` | no | `500` | Push interval in ms. Clamped to minimum 50. |
| `settings.sendOnChange` | no | `false` | Skip the push entirely when no values have changed since the last delivery. |
| `fields` | no | `{}` | Map of user-defined response key → registry key. Empty map produces no data. |

---

### `unsubscribe`

Stops the current push subscription. Sending `"subscribe"` again restarts it.

```json
{ "type": "unsubscribe" }
```

---

### `query`

Performs a **one-shot read** of the requested fields and returns a single
`"data"` message. Does not affect or require an active subscription.

```jsonc
{
  "type": "query",
  "fields": {
    "<alias>": "<registry key>",
    ...
  }
}
```

---

### `describe`

Returns the full list of field keys supported by the plugin, including their
value type and description.

```json
{ "type": "describe" }
```

---

## Server → Client messages

### `data`

Sent in response to a subscription push or a `"query"` request.

```jsonc
{
  "type": "data",
  "ts": 1712462400123,   // Unix timestamp in milliseconds
  "fields": {
    "<alias>": <float>,  // one entry per successfully resolved field
    ...
  }
}
```

### `describe`

Sent in response to a `"describe"` request.

```jsonc
{
  "type": "describe",
  "fields": {
    "ActorValue::kHealth": { "valueType": "float", "description": "Current health points" },
    ...
  }
}
```

### `error`

Sent when a message cannot be processed. The current subscription (if any) is
**not** cancelled on error.

```jsonc
{
  "type": "error",
  "message": "Unknown field key: 'ActorValue::kBanana'"
}
```

---

## Available field keys

Field values are either `float` (all `ActorValue::*` keys) or JSON `array` / `integer` (all `Inventory::*` keys).
Fields of different types can be freely mixed in a single `subscribe` or `query` message.

### ActorValue fields — `float`

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

Each `ActorValue` key also has `::Base`, `::Permanent`, and `::Clamped` variants
(e.g. `ActorValue::kHealth::Base`).

### Inventory fields — `array` / `integer`

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

**`Inventory::Categories` element shape:**
```jsonc
{ "categoryId": "Weapons", "name": "Weapons", "count": 5 }
```
- `categoryId` — stable internal identifier, always English (e.g. `"Weapons"`, `"Potions"`)
- `name` — in-game localized display name when available via GMST, otherwise equals `categoryId`

**`Inventory::Items::*` base element shape (all categories):**
```jsonc
{ "name": "Iron Sword", "formId": "0x00012EB7", "count": 1, "weight": 9.0, "value": 25, "isFavorite": false, "isStolen": false }
```
- `isStolen` — `true` when the item stack carries a stolen (red-hand) flag

Additional fields per category:

| Category | Extra fields |
|---|---|
| Weapons | `isEquipped`, `baseDamage` (float — weapon base damage before perks), `damage` (float — effective damage = `baseDamage × kAttackDamageMult`), `enchantment` (object or null — see below), `enchantmentCharge` (number or null) |
| Apparel | `isEquipped`, `armorTypeId` (`"Heavy"` / `"Light"` / `"Clothing"` — stable key), `armorType` (localized in-game display name via GMST `sSkillHeavyarmor`/`sSkillLightarmor`), `baseArmorRating` (float — raw form value before perks), `armorRating` (float — effective value as shown in inventory = `baseArmorRating × (1 + kArmorPerks/100)`), `bodySlots` (array of strings — e.g. `["Body", "Forearms"]`), `enchantment` (object or null — see below) |
| Potions | `effects` (array — see below) |
| Food | `effects` (array — see below) |
| Ingredients | `effects` — array of `{ "name": string, "known": bool }` (up to 4 entries) |
| Books | `description` (string from game's TESDescription) |
| Scrolls | `effects` (array — see below) |
| SoulGems | `capacity` (string — max soul level: `"Petty"`, `"Lesser"`, `"Common"`, `"Greater"`, `"Grand"`, or `"None"`), `containedSoul` (string — current fill level, same values) |
| Favorites | `isEquipped`, `type` (categoryId string, e.g. `"Weapons"`) |

**Enchantment object** (used by Weapons and Apparel — `null` when no enchantment):
```jsonc
{
  "name": "Fiery Soul Trap",
  "effects": [
    { "name": "Fire Damage", "magnitude": 10.0, "duration": 1, "descriptionTemplate": "Do <mag> points of fire damage." },
    { "name": "Soul Trap",   "magnitude": 0.0,  "duration": 5, "descriptionTemplate": "If target dies within <dur> seconds, fills a soul gem." }
  ]
}
```

**Effects array element** (used by Potions, Food, Scrolls, and inside enchantment objects):
```jsonc
{ "name": "Restore Health", "magnitude": 50.0, "duration": 0, "descriptionTemplate": "Restore <mag> points of Health." }
```

- `name` — localized effect name from the game (e.g. `"Restore Health"` / `"Восстановление здоровья"`)
- `magnitude` — effect magnitude; substitute for `<mag>` in `descriptionTemplate`
- `duration` — effect duration in seconds (0 = instant); substitute for `<dur>` in `descriptionTemplate`
- `descriptionTemplate` — localized description template from the `EffectSetting` DNAM field (`magicItemDescription`); may contain `<mag>` and `<dur>` placeholders that the client should replace with `magnitude` and `duration`

Use `{ "type": "describe" }` at runtime to get the full list with descriptions.

---

## Examples

### Example 1 — Combat HUD overlay (subscribe, push every 200 ms)

A client wants to display a real-time HUD showing the player's three vitals,
updated 5 times per second. The response field names match the HUD variables
in the client application.

**Client sends:**
```json
{
  "type": "subscribe",
  "settings": {
    "frequency": 200
  },
  "fields": {
    "hp":  "ActorValue::kHealth",
    "mp":  "ActorValue::kMagicka",
    "sta": "ActorValue::kStamina"
  }
}
```

**Server replies (every 200 ms):**
```json
{
  "type": "data",
  "ts": 1712462400123,
  "fields": {
    "hp":  320.5,
    "mp":  180.0,
    "sta": 99.25
  }
}
```

---

### Example 2 — Skill tracker with `sendOnChange`

A client shows a skill progress panel. Skills change rarely, so bandwidth
should not be wasted when nothing has changed.

**Client sends:**
```json
{
  "type": "subscribe",
  "settings": {
    "frequency": 1000,
    "sendOnChange": true
  },
  "fields": {
    "sneak":      "ActorValue::kSneak",
    "lockpick":   "ActorValue::kLockpicking",
    "pickpocket": "ActorValue::kPickpocket",
    "speech":     "ActorValue::kSpeech"
  }
}
```

The server polls every 1 second, but only delivers a `"data"` message when at
least one skill value has changed. While the player is standing still, nothing
is sent.

---

### Example 3 — One-shot query (no subscription needed)

A client needs a snapshot of resistances when the player opens a character
sheet. It uses `"query"` instead of subscribing to avoid unnecessary traffic.

**Client sends:**
```json
{
  "type": "query",
  "fields": {
    "fireRes":   "ActorValue::kResistFire",
    "frostRes":  "ActorValue::kResistFrost",
    "shockRes":  "ActorValue::kResistShock",
    "magicRes":  "ActorValue::kResistMagic",
    "poisonRes": "ActorValue::kPoisonResist"
  }
}
```

**Server replies once:**
```json
{
  "type": "data",
  "ts": 1712462401000,
  "fields": {
    "fireRes":   25.0,
    "frostRes":  0.0,
    "shockRes":  10.0,
    "magicRes":  15.0,
    "poisonRes": 0.0
  }
}
```

---

### Example 4 — Discover available fields

**Client sends:**
```json
{ "type": "describe" }
```

**Server replies:**
```jsonc
{
  "type": "describe",
  "fields": {
    "ActorValue::kHealth":    { "valueType": "float",   "valueCategory": "current", "description": "Current health points" },
    "ActorValue::kMagicka":   { "valueType": "float",   "valueCategory": "current", "description": "Current magicka points" },
    "Inventory::Categories":  { "valueType": "array",   "description": "Array of inventory categories with item counts" },
    "Inventory::Gold":        { "valueType": "integer", "description": "Player's current gold amount" },
    // ... all registered fields
  }
}
```

---

### Example 5 — Mixed-type subscription (vitals + inventory)

A client wants to display a HUD that shows both the player's health and their
current weapon loadout in a single push stream.

**Client sends:**
```json
{
  "type": "subscribe",
  "settings": { "frequency": 1000, "sendOnChange": true },
  "fields": {
    "hp":      "ActorValue::kHealth",
    "cats":    "Inventory::Categories",
    "weapons": "Inventory::Items::Weapons"
  }
}
```

**Server replies (whenever any value changes):**
```json
{
  "type": "data",
  "ts": 1712462400123,
  "fields": {
    "hp": 320.5,
    "cats": [
      { "categoryId": "Weapons", "name": "Weapons", "count": 2 },
      { "categoryId": "Apparel", "name": "Apparel", "count": 7 }
    ],
    "weapons": [
      { "name": "Iron Sword",    "formId": "0x00012EB7", "count": 1, "weight": 9.0,  "value": 25  },
      { "name": "Steel Dagger",  "formId": "0x00013CE6", "count": 1, "weight": 2.5,  "value": 22  }
    ]
  }
}
```

---

## Configuration

The plugin reads an optional INI file from the same directory as the DLL:

```
Data/SKSE/Plugins/SkyrimWebSocket.ini
```

If the file does not exist, the defaults shown below are used.

```ini
[Server]
; Address the WebSocket server binds to.
; Use 127.0.0.1 (default) to accept connections from localhost only.
; Use 0.0.0.0 to accept connections from any network interface (useful for debugging from a remote client).
ListenAddress=127.0.0.1

; TCP port the WebSocket server listens on.
; Default: 8765
Port=8765
```

An annotated example file `SkyrimWebSocket.ini.example` is included in the repository root.
Copy it to `Data/SKSE/Plugins/SkyrimWebSocket.ini` and edit as needed.
