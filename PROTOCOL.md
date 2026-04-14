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

Starts (or replaces) a push subscription identified by `"id"`. Multiple
subscriptions with different IDs can coexist on the same connection. The server
will send a `"data"` message at the requested frequency for as long as the
session is open or until `"unsubscribe"` is received for that ID.

```jsonc
{
  "type": "subscribe",
  "id": "my-sub",           // unique subscription identifier (required)
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
| `id` | **yes** | — | Unique identifier for this subscription. Sending `subscribe` with an existing `id` replaces that subscription. |
| `settings.frequency` | no | `500` | Push interval in ms. Clamped to minimum 50. |
| `settings.sendOnChange` | no | `false` | Skip the push entirely when no values have changed since the last delivery. |
| `fields` | no | `{}` | Map of user-defined response key → registry key. Empty map produces no data. |

---

### `unsubscribe`

Stops a specific subscription by ID. If `"id"` is omitted, all active
subscriptions are cancelled.

```jsonc
// Stop a specific subscription:
{ "type": "unsubscribe", "id": "my-sub" }

// Stop all subscriptions:
{ "type": "unsubscribe" }
```

---

### `query`

Performs a **one-shot read** of the requested fields and returns a single
`"data"` message. Does not affect or require an active subscription.

```jsonc
{
  "type": "query",
  "id": "my-query",   // unique request identifier (required)
  "fields": {
    "<alias>": "<registry key>",
    ...
  }
}
```

| Field | Required | Default | Description |
|---|---|---|---|
| `id` | **yes** | — | Identifier echoed back in the `"data"` response so the client can match the reply to the request. |
| `fields` | **yes** | — | Map of user-defined response key → registry key. |

---

### `unsubscribe_all`

Stops **all** active subscriptions at once.

```json
{ "type": "unsubscribe_all" }
```

---

### `heartbeat`

Sent by the client periodically (recommended: every 1 second) to verify that
the server is alive and reachable. The server replies immediately with a
`"heartbeat"` message containing the current server timestamp.

```json
{ "type": "heartbeat" }
```

---

### `command`

Sends a game command (e.g. equip, unequip, use, drop, favorite). The server
validates the request and executes it on the game thread, then replies with a
`"commandResult"` message.

```jsonc
{
  "type": "command",
  "id": "cmd-1",            // unique request identifier (required)
  "command": "equip",       // command name (required): equip | unequip | use | drop | favorite
  "formId": "0x00012EB7",  // item form ID as hex string (required)
  "hand": "right",          // equip/unequip hand: "right" or "left" (optional, weapons only, default: "right")
  "count": 1                // drop count (optional, default: 1, only used by "drop")
}
```

| Field | Required | Default | Description |
|---|---|---|---|
| `id` | **yes** | — | Unique identifier echoed back in the `"commandResult"` response. |
| `command` | **yes** | — | One of: `equip`, `unequip`, `use`, `drop`, `favorite`. |
| `formId` | **yes** | — | Hex form ID of the target item (e.g. `"0x00012EB7"`). |
| `hand` | no | `"right"` | Target hand for weapons: `"right"` or `"left"`. Ignored for non-weapon items. Two-handed weapons only accept `"right"`. |
| `count` | no | `1` | Number of items to drop. Only used by the `drop` command. |

#### Command details

| Command | Applies to | Behaviour |
|---|---|---|
| `equip` | Weapons, Apparel, Ammo | Equips the item. Weapons use the `hand` parameter to select left/right hand. Apparel and ammo auto-select the correct slot. |
| `unequip` | Weapons, Apparel, Ammo | Removes the equipped item. For weapons, `hand` specifies which hand to unequip from. |
| `use` | Potions, Food, Ingredients, Scrolls | Consumes the item (applies effect). Scrolls are equipped for casting. |
| `drop` | Any item | Drops `count` items from inventory onto the ground. |
| `favorite` | Any item | Toggles the item's favorite status on/off. |

---

## Server → Client messages

### `data`

Sent in response to a subscription push or a `"query"` request.

```jsonc
{
  "type": "data",
  "id": "my-sub",          // subscription id or query id
  "ts": 1712462400123,     // Unix timestamp in milliseconds
  "fields": {
    "<alias>": <float | integer | string | array>,    // one entry per successfully resolved field
    ...
  }
}
```

The `"id"` field always mirrors the `"id"` from the originating `"subscribe"` or `"query"` message, allowing the client to route responses correctly.

### `commandResult`

Sent in response to a `"command"` request. Reports whether the command
succeeded or failed.

```jsonc
// Success:
{
  "type": "commandResult",
  "id": "cmd-1",
  "success": true
}

// Failure:
{
  "type": "commandResult",
  "id": "cmd-1",
  "success": false,
  "error": "Item not in inventory"
}
```

| Field | Type | Description |
|---|---|---|
| `id` | string | Mirrors the `"id"` from the originating `"command"` message. |
| `success` | bool | `true` if the command executed without error. |
| `error` | string | Present only when `success` is `false`. Human-readable error description. |

### `heartbeat`

Sent in response to a client `"heartbeat"` request.

```jsonc
{
  "type": "heartbeat",
  "ts": 1712462400123   // Unix timestamp in milliseconds (server time)
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

Field values are `float` (all `ActorValue::*` keys), JSON `array` / `integer` (all `Inventory::*` keys), or `string` (all `Game::*` keys).
Fields of different types can be freely mixed in a single `subscribe` or `query` message.

**For a complete list of available fields, see:**
- [docs/ActorValue.md](docs/ActorValue.md) — All ActorValue fields and value type modifiers
- [docs/Inventory.md](docs/Inventory.md) — All Inventory fields with detailed response structures
- [docs/Player.md](docs/Player.md) — Character level, XP, and inventory weight fields
- [docs/Game.md](docs/Game.md) — Game-level settings such as the current language

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
  "id": "vitals",
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
  "id": "vitals",
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
  "id": "skills",
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
  "id": "resistances",
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
  "id": "resistances",
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

### Example 4 — Connection keep-alive via heartbeat

A client sends a heartbeat every second to confirm the server is running.

**Client sends (every 1 s):**
```json
{ "type": "heartbeat" }
```

**Server replies immediately:**
```json
{
  "type": "heartbeat",
  "ts": 1712462402000
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
  "id": "hud",
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
  "id": "hud",
  "ts": 1712462400123,
  "fields": {
    "hp": 320.5,
    "cats": [
      { "categoryId": "Weapons", "name": "Weapons", "count": 2 },
      { "categoryId": "Apparel", "name": "Apparel", "count": 7 }
    ],
    "weapons": [
      { "name": "Iron Sword",    "formId": "0x00012EB7", "categoryId": "Weapons", "count": 1, "weight": 9.0,  "value": 25  },
      { "name": "Steel Dagger",  "formId": "0x00013CE6", "categoryId": "Weapons", "count": 1, "weight": 2.5,  "value": 22  }
    ]
  }
}
```

---

### Example 6 — Multiple concurrent subscriptions

A client subscribes to two independent streams: fast vitals and slow-changing
skill values. Both use the same WebSocket connection.

**Client sends (subscription 1):**
```json
{
  "type": "subscribe",
  "id": "vitals",
  "settings": { "frequency": 100 },
  "fields": {
    "hp":  "ActorValue::kHealth",
    "mp":  "ActorValue::kMagicka",
    "sta": "ActorValue::kStamina"
  }
}
```

**Client sends (subscription 2):**
```json
{
  "type": "subscribe",
  "id": "skills",
  "settings": { "frequency": 2000, "sendOnChange": true },
  "fields": {
    "sneak":    "ActorValue::kSneak",
    "lockpick": "ActorValue::kLockpicking"
  }
}
```

The server sends `"data"` messages tagged with the respective `"id"` at
independent intervals. To stop only the skill subscription:

```json
{ "type": "unsubscribe", "id": "skills" }
```

---

### Example 7 — Equip a one-handed weapon to the left hand

**Client sends:**
```json
{
  "type": "command",
  "id": "equip-sword",
  "command": "equip",
  "formId": "0x00012EB7",
  "hand": "left"
}
```

**Server replies:**
```json
{
  "type": "commandResult",
  "id": "equip-sword",
  "success": true
}
```

---

### Example 8 — Use a potion

**Client sends:**
```json
{
  "type": "command",
  "id": "use-potion",
  "command": "use",
  "formId": "0x00039BE5"
}
```

**Server replies:**
```json
{
  "type": "commandResult",
  "id": "use-potion",
  "success": true
}
```

---

### Example 9 — Drop multiple items

**Client sends:**
```json
{
  "type": "command",
  "id": "drop-arrows",
  "command": "drop",
  "formId": "0x0003BE11",
  "count": 10
}
```

**Server replies:**
```json
{
  "type": "commandResult",
  "id": "drop-arrows",
  "success": true
}
```

---

### Example 10 — Toggle favorite on an item

**Client sends:**
```json
{
  "type": "command",
  "id": "fav-sword",
  "command": "favorite",
  "formId": "0x00012EB7"
}
```

**Server replies:**
```json
{
  "type": "commandResult",
  "id": "fav-sword",
  "success": true
}
```

---

### Example 11 — Command validation error

**Client sends (item not in inventory):**
```json
{
  "type": "command",
  "id": "bad-equip",
  "command": "equip",
  "formId": "0xDEADBEEF"
}
```

**Server replies:**
```json
{
  "type": "commandResult",
  "id": "bad-equip",
  "success": false,
  "error": "Item not in inventory"
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
