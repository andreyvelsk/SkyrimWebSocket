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

**For a complete list of available fields, see:**
- [docs/ActorValue.md](docs/ActorValue.md) — All ActorValue fields and value type modifiers
- [docs/Inventory.md](docs/Inventory.md) — All Inventory fields with detailed response structures
- [docs/Player.md](docs/Player.md) — Character level, XP, and inventory weight fields

Use `{ "type": "describe" }` at runtime to get the full list of all available fields with descriptions.

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
