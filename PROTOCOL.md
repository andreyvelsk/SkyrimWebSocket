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

All ActorValue fields return `float` values. For a complete list of available fields with detailed descriptions and value type modifiers, see [ACTORVALUE.md](docs/ACTORVALUE.md).

### Quick Reference

Common field examples:
- `ActorValue::kHealth` — Current health points
- `ActorValue::kHealth::Base` — Base health (without modifications)
- `ActorValue::kMagicka` — Current magicka
- `ActorValue::kStamina` — Current stamina
- All skill fields (e.g., `ActorValue::kSneak`, `ActorValue::kOneHanded`)
- All resistance fields (e.g., `ActorValue::kResistFire`, `ActorValue::kResistMagic`)

Use `{ "type": "describe" }` at runtime to get the full updated list with all available fields.

---

## Inventory field keys

Inventory fields return complex JSON objects. For a complete reference with detailed field structures, see [INVENTORY.md](docs/INVENTORY.md).

### Quick Reference

| Registry key | Description |
|---|---|
| `Inventory::kAll` | Complete player inventory grouped by type |
| `Inventory::kWeapon` | All weapons in inventory |
| `Inventory::kArmor` | All armor pieces in inventory |
| `Inventory::kAmmo` | All ammunition |
| `Inventory::kPotion` | All potions and poisons |
| `Inventory::kFood` | All food and drinks |
| `Inventory::kBook` | All books and spell tomes |
| `Inventory::kIngredient` | All alchemy ingredients |
| `Inventory::kMisc` | All miscellaneous items |

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
    "ActorValue::kHealth":    { "valueType": "float", "description": "Current health points" },
    "ActorValue::kMagicka":   { "valueType": "float", "description": "Current magicka points" },
    // ... all registered fields
  }
}
```

---

### Example 5 — Query player inventory

**Client sends:**
```json
{
  "type": "query",
  "fields": {
    "weapons": "Inventory::kWeapon"
  }
}
```

A typical response includes detailed weapon information. For complete response structure details, see [INVENTORY.md](docs/INVENTORY.md).

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
