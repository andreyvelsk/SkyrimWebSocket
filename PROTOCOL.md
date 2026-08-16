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

Sends a game command. The server validates the request, executes it on the
game thread, and replies with a `"commandResult"` message.

#### Common envelope

Every `command` message shares the same three top-level fields. Additional
fields are command-specific and are documented in the per-command sections
below.

| Field | Required | Description |
|---|---|---|
| `type` | **yes** | Always the literal string `"command"`. |
| `id` | **yes** | Unique identifier echoed back in the `"commandResult"` response. |
| `command` | **yes** | The command name. See the list below. |

#### Command catalogue

| Command | Purpose | Section |
|---|---|---|
| `equip` | Equip an inventory item (weapon / apparel / ammo). | [↓](#equip) |
| `unequip` | Unequip an item. | [↓](#unequip) |
| `use` | Consume a potion / food / ingredient / scroll. | [↓](#use) |
| `read_book` | Open and read a book from inventory. | [↓](#read_book) |
| `drop` | Drop one or more inventory items onto the ground. | [↓](#drop) |
| `favorite` | Toggle the favorite flag on an inventory item. | [↓](#favorite) |
| `texture_preview` | Produce a base64 PNG preview from a raw DDS texture path. | [↓](#texture_preview) |
| `file_download` | Download an arbitrary file (BSA / loose) as base64. | [↓](#file_download) |
| `equip_spell` | Equip a known spell to a hand. | [↓](#equip_spell) |
| `unequip_spell` | Unequip a spell from a hand. | [↓](#unequip_spell) |
| `favorite_spell` | Toggle the favorite flag on a known spell or power. | [↓](#favorite_spell) |
| `equip_shout` | Equip a known dragon shout to the voice slot. | [↓](#equip_shout) |
| `unequip_shout` | Remove a dragon shout from the voice slot. | [↓](#unequip_shout) |
| `equip_power` | Equip a known power or lesser power to the voice slot. | [↓](#equip_power) |
| `favorite_shout` | Toggle the favorite flag on a known dragon shout. | [↓](#favorite_shout) |
| `hotkey_set` | Bind an item or spell to one of the 8 hotkey slots. | [↓](#hotkey_set) |
| `hotkey_clear` | Clear a hotkey slot. | [↓](#hotkey_clear) |
| `hotkey_trigger` | Fire the action bound to a hotkey slot. | [↓](#hotkey_trigger) |
| `quest_set_active` | Set or clear a quest's active/tracked state. | [↓](#quest_set_active) |
| `player_marker_set` | Place / move the player's custom map marker. | [↓](#player_marker_set) |
| `player_marker_clear` | Hide the player's custom map marker. | [↓](#player_marker_clear) |
| `fast_travel` | Teleport the player to a discovered map marker. | [↓](#fast_travel) |

---

#### `equip`

Equips an inventory item.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the item to equip. Must be present in inventory. |
| `hand` | no | `"right"` | Weapons only: `"right"` or `"left"`. Two-handed weapons only accept `"right"`. Ignored for apparel and ammo (slot is auto-selected). |

**Applies to:** Weapons, Apparel, Ammo.

```json
{
  "type": "command",
  "id": "equip-sword",
  "command": "equip",
  "formId": "0x00012EB7",
  "hand": "left"
}
```

---

#### `unequip`

Removes an equipped item.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the item to unequip. |
| `hand` | no | `"right"` | Weapons only: which hand to unequip from. Ignored for apparel and ammo. |

**Applies to:** Weapons, Apparel, Ammo.

```json
{
  "type": "command",
  "id": "unequip-sword",
  "command": "unequip",
  "formId": "0x00012EB7",
  "hand": "left"
}
```

---

#### `use`

Consumes a usable item (applies its effect). Scrolls are equipped for casting
instead of being consumed immediately.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the item to use. Must be present in inventory. |

**Applies to:** Potions, Food, Ingredients, Scrolls.

```json
{
  "type": "command",
  "id": "use-potion",
  "command": "use",
  "formId": "0x00039BE5"
}
```

---

#### `read_book`

Opens the book reading UI for a book currently in the player's inventory.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the book to read. Must be present in inventory. |

**Applies to:** Books.

```json
{
  "type": "command",
  "id": "read-book",
  "command": "read_book",
  "formId": "0x0001AFD3"
}
```

---

#### `drop`

Drops items from the inventory onto the ground in front of the player.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the item to drop. |
| `count` | no | `1` | Number of copies to drop. Clamped to the amount actually owned. |

**Applies to:** Any inventory item.

```json
{
  "type": "command",
  "id": "drop-arrows",
  "command": "drop",
  "formId": "0x0003BE11",
  "count": 10
}
```

---

#### `favorite`

Toggles the favorite flag on an inventory item.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the item. Must be present in inventory. |

**Applies to:** Any inventory item.

```json
{
  "type": "command",
  "id": "fav-sword",
  "command": "favorite",
  "formId": "0x00012EB7"
}
```

---

---

#### `texture_preview`

Produces a base64-encoded PNG preview from a raw DDS texture path. This is the
generic primitive for any texture asset (item icons, map marker icons, book
art, etc.) and is also useful for testing the DDS→PNG pipeline.

| Field | Required | Default | Description |
|---|---|---|---|
| `path` | **yes** | — | DDS path relative to the game `Data` folder, e.g. `"textures/interface/icons/weapons/ironsword.dds"`. Backslashes are accepted. |

**Response** — the `data` object of `commandResult`:

| Field | Type | Description |
|---|---|---|
| `mimeType` | string | Always `"image/png"`. |
| `width` | integer | Image width in pixels. |
| `height` | integer | Image height in pixels. |
| `imageBase64` | string | Base64-encoded PNG bytes (no `data:` URI prefix). |

> The conversion runs off the game thread.

```json
{
  "type": "command",
  "id": "t1",
  "command": "texture_preview",
  "path": "textures/interface/icons/weapons/ironsword.dds"
}
```

---

#### `file_download`

Downloads an arbitrary file from the game's virtual filesystem (BSA archives
and loose files) and returns it as base64. Useful for any non-texture asset:
`.txt`, `.swf`, `.nif`, `.pex`, etc.

| Field | Required | Default | Description |
|---|---|---|---|
| `path` | **yes** | — | File path relative to the game `Data` folder, e.g. `"interface/skyui/skyui.swf"` or `"textures/interface/icons/weapons/ironsword.dds"`. Backslashes are accepted. |

**Response** — the `data` object of `commandResult`:

| Field | Type | Description |
|---|---|---|
| `mimeType` | string | MIME type guessed from the file extension (e.g. `"application/x-shockwave-flash"` for `.swf`, `"text/plain"` for `.txt`). |
| `size` | integer | File size in bytes. |
| `dataBase64` | string | Base64-encoded raw file bytes. |

```json
{
  "type": "command",
  "id": "f1",
  "command": "file_download",
  "path": "interface/skyui/skyui.swf"
}
```

```jsonc
// Success response:
{
  "type": "commandResult",
  "id": "f1",
  "success": true,
  "data": {
    "mimeType": "application/x-shockwave-flash",
    "size": 51234,
    "dataBase64": "..."
  }
}
```

---

#### `equip_spell`

Equips a known spell to a hand for casting.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the spell. Must be known by the player. |
| `hand` | no | `"right"` | `"right"` or `"left"`. Master-level spells always equip to both hands and only accept `"right"`. Equipping a non-master spell to the opposite hand while it is already in the first will dual-cast it. |

**Applies to:** Spells known by the player.

```json
{
  "type": "command",
  "id": "equip-fireball",
  "command": "equip_spell",
  "formId": "0x0000A23E",
  "hand": "right"
}
```

---

#### `unequip_spell`

Unequips a spell from a hand.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the spell. Must currently be equipped. |
| `hand` | no | `"right"` | `"right"` or `"left"`. For dual-cast non-master spells, only the specified hand is cleared. Master-level spells are always removed from both hands. |

**Applies to:** Currently equipped spells.

```json
{
  "type": "command",
  "id": "unequip-fireball-right",
  "command": "unequip_spell",
  "formId": "0x0000A23E",
  "hand": "right"
}
```

---

#### `favorite_spell`

Toggles the favorite flag on a known spell or power. Favorited forms appear in the
magic favorites list of the spell menu.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the spell or power. Must be known by the player. |

**Applies to:** Spells, powers, and lesser powers known by the player.

```json
{
  "type": "command",
  "id": "fav-spell",
  "command": "favorite_spell",
  "formId": "0x0000A23E"
}
```

---

#### `equip_shout`

Equips a known dragon shout to the player's voice slot (replaces the currently
equipped shout).

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the shout. Must be known by the player. |

**Applies to:** Dragon shouts known by the player.

```json
{
  "type": "command",
  "id": "equip-shout",
  "command": "equip_shout",
  "formId": "0x0002F7BB"
}
```

---

#### `unequip_shout`

Removes a dragon shout from the player's voice slot. Fails if the specified
shout is not currently equipped.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the shout to unequip. Must be the shout currently in the voice slot. |

**Applies to:** The dragon shout currently equipped in the voice slot.

```json
{
  "type": "command",
  "id": "unequip-shout",
  "command": "unequip_shout",
  "formId": "0x0002F7BB"
}
```

---

#### `equip_power`

Equips a known power or lesser power to the player's voice slot (replaces
the currently equipped power/shout).

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the power. Must be known by the player. |

**Applies to:** Powers and lesser powers known by the player.

```json
{
  "type": "command",
  "id": "equip-power",
  "command": "equip_power",
  "formId": "0x000581F4"
}
```

---

#### `favorite_shout`

Toggles the favorite flag on a known dragon shout. Favorited shouts appear in
the favorites list accessible via the magic menu and can be bound to hotkeys.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the shout. Must be known by the player. |

**Applies to:** Dragon shouts known by the player.

```json
{
  "type": "command",
  "id": "fav-shout",
  "command": "favorite_shout",
  "formId": "0x0002F7BB"
}
```

---

#### `hotkey_set`

Binds an item or spell to one of the 8 hotkey slots. Replaces any previous
binding for the slot and automatically favourites the target if needed.

| Field | Required | Default | Description |
|---|---|---|---|
| `slot` | **yes** | — | Hotkey slot in the range `1..8`. |
| `formId` | **yes** | — | Hex form ID of the item or spell to bind. For items, must be present in inventory. For spells/shouts/powers, must be known by the player and of a hotkeyable type (spell, power, lesser power, voice power, shout). |

**Applies to:** Inventory items, spells, shouts, powers.

```json
{
  "type": "command",
  "id": "hk-set-1",
  "command": "hotkey_set",
  "slot": 1,
  "formId": "0x0000A23E"
}
```

---

#### `hotkey_clear`

Removes the binding on the given hotkey slot. No-op if the slot is already
empty.

| Field | Required | Default | Description |
|---|---|---|---|
| `slot` | **yes** | — | Hotkey slot in the range `1..8`. |

```json
{
  "type": "command",
  "id": "hk-clear-1",
  "command": "hotkey_clear",
  "slot": 1
}
```

---

#### `hotkey_trigger`

Fires the action bound to a hotkey slot by synthesizing a `Hotkey<N>` button
event through the engine's own `FavoritesHandler`. Behaviour is bit-for-bit
identical to the player physically pressing the corresponding number key:
spells toggle right-hand → left-hand → unequip, weapons toggle equip ↔
unequip (with two-handed / dual-wield rules), shouts and powers go to the
voice slot, consumables are used, etc. Always succeeds when `slot` is valid;
empty slots are silently ignored, just like vanilla.

| Field | Required | Default | Description |
|---|---|---|---|
| `slot` | **yes** | — | Hotkey slot in the range `1..8`. |

```json
{
  "type": "command",
  "id": "hk-trigger-1",
  "command": "hotkey_trigger",
  "slot": 1
}
```

---

#### `quest_set_active`

Sets or clears the active/tracked state of a quest. This is the same state the
journal uses to decide whether a quest contributes active quest-target markers.
Returns the updated quest entry in `data` using the `Player::Quests` object
shape.

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the quest. Use `questFormId` from `Player::Quests`. |
| `active` | **yes** | — | `true` to track/activate the quest, `false` to clear the active marker. |

```json
{
  "type": "command",
  "id": "track-quest",
  "command": "quest_set_active",
  "formId": "0x00036192",
  "active": true
}
```

---

#### `player_marker_set`

Places (or moves) the player's custom map marker and makes it visible /
fast-travel-enabled. Coordinates are in the marker's worldspace — in vanilla
play that is the global parent worldspace (Tamriel). Returns the new marker
state in `data` (same shape as the `Player::Marker` field). Fails if the
marker reference has not been initialized yet — open the world map at least
once in the save before calling this command.

| Field | Required | Default | Description |
|---|---|---|---|
| `x` | **yes** | — | X coordinate in the marker's worldspace. |
| `y` | **yes** | — | Y coordinate in the marker's worldspace. |
| `z` | no | `0` | Z coordinate. The marker is displayed on the world map regardless of Z. |

```json
{
  "type": "command",
  "id": "marker-set",
  "command": "player_marker_set",
  "x": 18000.0,
  "y": -15200.0
}
```

See [docs/Player.md](docs/Player.md) for full response details.

---

#### `player_marker_clear`

Hides the player's custom map marker. The underlying reference is preserved
and reused next time `player_marker_set` is called. Always succeeds. Returns
the resulting marker state in `data` (with `isSet: false`).

This command takes no parameters beyond the common envelope.

```json
{
  "type": "command",
  "id": "marker-clear",
  "command": "player_marker_clear"
}
```

---

#### `fast_travel`

Teleports the player to a discovered map marker. Mirrors the engine's
pre-flight checks: the marker must exist, be visible (discovered), have
`canFastTravel=true`, the player must not be in combat, and fast travel must
not be globally disabled by the worldspace. Returns the destination marker
info in `data` (`refId`, `name`, `typeId`, `x`, `y`, `isVisible`,
`canFastTravel`).

| Field | Required | Default | Description |
|---|---|---|---|
| `formId` | **yes** | — | Hex form ID of the map-marker reference. Use the `refId` value returned by `Map::Markers::Locations`. |

```json
{
  "type": "command",
  "id": "ft-1",
  "command": "fast_travel",
  "formId": "0x000136D5"
}
```

See [docs/Map.md](docs/Map.md) for the full pre-flight check list and
response details.

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

Field values are `float` (all `ActorValue::*` keys), JSON `array` / `object` /
`integer` (complex `Inventory::*`, `Hotkey::*`, `Player::*`, `Map::*`, and
`Magic::*` keys), or `string` (string-valued `Game::*` keys).
Fields of different types can be freely mixed in a single `subscribe` or `query` message.

**For a complete list of available fields, see:**
- [docs/ActorValue.md](docs/ActorValue.md) — All ActorValue fields and value type modifiers
- [docs/Inventory.md](docs/Inventory.md) — All Inventory fields with detailed response structures
- [docs/Player.md](docs/Player.md) — Character level, XP, and inventory weight fields
- [docs/Game.md](docs/Game.md) — Game-level settings such as the current language
- [docs/Magic.md](docs/Magic.md) — All Magic fields with spell information and status
- [docs/Hotkeys.md](docs/Hotkeys.md) — Hotkey slot bindings (`Hotkey::Items`)

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
      { "name": "Iron Sword",    "formId": "0x00012EB7", "count": 1, "weight": 9.0,  "value": 25  },
      { "name": "Steel Dagger",  "formId": "0x00013CE6", "count": 1, "weight": 2.5,  "value": 22  }
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

### Example 7 — Command validation error

Per-command request examples are inlined in each command's section above.
This example shows the failure-response shape, which is the same for every
command.

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
