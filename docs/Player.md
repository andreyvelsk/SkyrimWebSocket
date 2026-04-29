# Player Fields Reference

Player fields expose character-level stats that are not available through the
`ActorValue` system: character level, experience points, inventory weight, and
world position.

All values are returned as `float`, `integer`, or `object` as noted.

## Available Player Fields

| Registry key | Value type | Description |
|---|---|---|
| `Player::Level` | `integer` | Character level |
| `Player::XP::Current` | `float` | XP earned within the current level (resets to 0 on level-up) |
| `Player::XP::Next` | `float` | XP threshold required to reach the next level |
| `Player::XP::LevelStart` | `float` | XP value at the start of the current level (always `0.0`; provided for progress-bar math) |
| `Player::InventoryWeight` | `float` | Total weight of all items currently in the player's inventory |
| `Player::CarryWeight` | `float` | Maximum carry weight (same value as `ActorValue::kCarryWeight`) |
| `Player::Position` | `object` | Player position, heading, current worldspace and cell — see below |
| `Player::ExteriorPosition` | `object` | Last known exterior position (for global-map rendering while in interiors / city sub-worlds) — see below |
| `Player::Marker` | `object` | Player-placed custom map marker state (the marker dropped by clicking on the world map) — see below |

---

## Building an XP progress bar

Use the three `Player::XP::*` fields together:

```
progress = (XP::Current - XP::LevelStart) / (XP::Next - XP::LevelStart)
         = XP::Current / XP::Next   ← simplified, since LevelStart is always 0
```

Because `XP::LevelStart` is always `0.0` in Skyrim (the engine resets XP to zero
on each level-up), the formula simplifies to `XP::Current / XP::Next`. The
`XP::LevelStart` field is still included so that clients can apply the general
formula without special-casing.

### Example — XP progress bar subscription

```json
{
  "type": "subscribe",
  "settings": { "frequency": 1000, "sendOnChange": true },
  "fields": {
    "level":      "Player::Level",
    "xpCurrent":  "Player::XP::Current",
    "xpNext":     "Player::XP::Next",
    "xpStart":    "Player::XP::LevelStart"
  }
}
```

**Server reply:**
```json
{
  "type": "data",
  "ts": 1712462400123,
  "fields": {
    "level":     12,
    "xpCurrent": 500.0,
    "xpNext":    1000.0,
    "xpStart":   0.0
  }
}
```

Progress bar fill: `500.0 / 1000.0 = 50 %`.

---

## Building a carry-weight bar

```json
{
  "type": "query",
  "fields": {
    "currentWeight": "Player::InventoryWeight",
    "maxWeight":     "Player::CarryWeight"
  }
}
```

**Server reply:**
```json
{
  "type": "data",
  "ts": 1712462400200,
  "fields": {
    "currentWeight": 183.5,
    "maxWeight":     300.0
  }
}
```

---

## `Player::Position`

Returns the player's current position, heading, and the worldspace / cell they
are in.

### Object shape

| Field | Type | Description |
|---|---|---|
| `x` | `float` | X coordinate, **local to the current worldspace** (or interior cell) |
| `y` | `float` | Y coordinate, local to the current worldspace |
| `z` | `float` | Z coordinate (height) |
| `angle` | `float` | Z-axis rotation (yaw / heading) in **radians**. `0` = North, increases clockwise. Convert to degrees: `angle * 180 / π`. |
| `worldspace` | `string \| null` | EditorID of the current worldspace (e.g. `"Tamriel"`, `"WhiterunWorld"`, `"DLC2SolstheimWorld"`). `null` in interiors. |
| `worldspaceFormId` | `string \| null` | Hex form ID of the current worldspace, or `null` in interiors. |
| `parentWorldspace` | `string \| null` | EditorID of the **root** worldspace (walks `parentWorld` up to the top). For Tamriel-anchored sub-worlds (cities) this equals `"Tamriel"`. For top-level worlds (`Tamriel`, `DLC2SolstheimWorld`) equals `worldspace`. `null` in interiors. |
| `parentWorldspaceFormId` | `string \| null` | Hex form ID of the root worldspace. |
| `cell` | `string \| null` | EditorID of the current cell. |
| `cellFormId` | `string \| null` | Hex form ID of the current cell. |
| `isInterior` | `bool` | `true` if the player is inside an interior cell (a building, dungeon, etc.). |

### Mapping to a global map

Skyrim has many separate worldspaces, each with its own coordinate system:

- **`Tamriel`** — the global outdoor map.
- **`DLC2SolstheimWorld`** — Solstheim, a separate top-level world with its own map.
- **`WhiterunWorld`, `RiftenWorld`, `SolitudeWorld`, …** — city sub-worlds.
  Their `parentWorld` is `Tamriel`, but their `(x, y)` are in their own local
  coordinate system and **do not** match the global Tamriel map.
- **Interiors** (homes, caves, dungeons) have no worldspace at all and use the
  cell's local coordinates.

When rendering a global Tamriel map, only plot `Player::Position.x/y` directly
when `worldspace == "Tamriel"`. Otherwise, use [`Player::ExteriorPosition`](#playerexteriorposition)
to place the player at the last known Tamriel position (typically the marker
of the city / dungeon they entered).

### Example — high-frequency map position subscription

```json
{
  "type": "subscribe",
  "id": "map-pos",
  "settings": { "frequency": 100, "sendOnChange": true },
  "fields": {
    "pos":     "Player::Position",
    "extPos": "Player::ExteriorPosition"
  }
}
```

**Server push (player walking in Tamriel):**
```json
{
  "type": "data",
  "id": "map-pos",
  "ts": 1712462400123,
  "fields": {
    "pos": {
      "x": 18000.5, "y": -15200.3, "z": 312.0, "angle": 1.5707963,
      "worldspace": "Tamriel", "worldspaceFormId": "0x0000003C",
      "parentWorldspace": "Tamriel", "parentWorldspaceFormId": "0x0000003C",
      "cell": "Wilderness", "cellFormId": "0x00009BE9",
      "isInterior": false
    },
    "extPos": {
      "x": 18000.5, "y": -15200.3, "z": 312.0,
      "worldspace": "Tamriel", "worldspaceFormId": "0x0000003C",
      "parentWorldspace": "Tamriel", "parentWorldspaceFormId": "0x0000003C"
    }
  }
}
```

---

## `Player::ExteriorPosition`

Returns the **last known exterior position** that the game itself caches for
the compass and world map. Useful for keeping a marker on the global map
pinned to the city / dungeon entrance while the player is inside.

### Object shape

| Field | Type | Description |
|---|---|---|
| `x` | `float` | Last exterior X (in `worldspace` coordinates). |
| `y` | `float` | Last exterior Y. |
| `z` | `float` | Last exterior Z. |
| `worldspace` | `string \| null` | EditorID of the cached worldspace, or `null` if the game has not cached one yet. |
| `worldspaceFormId` | `string \| null` | Hex form ID. |
| `parentWorldspace` | `string \| null` | EditorID of the root worldspace (see `Player::Position`). |
| `parentWorldspaceFormId` | `string \| null` | Hex form ID of the root worldspace. |

### Recommended client logic for a global Tamriel map

```js
let x, y;
if (pos.parentWorldspace === "Tamriel" && !pos.isInterior
    && pos.worldspace === "Tamriel") {
  // Outside, in Tamriel proper — use live coordinates.
  x = pos.x;
  y = pos.y;
} else if (extPos.parentWorldspace === "Tamriel") {
  // Inside an interior or a Tamriel city sub-world — pin to the entrance.
  x = extPos.x;
  y = extPos.y;
} else {
  // Solstheim or another top-level world — switch maps or hide marker.
}
```

---

## `Player::Marker`

Returns the state of the **player-placed custom map marker** — the marker the
player can drop on the world map by clicking on a spot. The engine maintains a
single dedicated reference for it and toggles its visibility on/off rather than
creating new objects, so reading and writing always operate on the same marker.

### Object shape

| Field | Type | Description |
|---|---|---|
| `isSet` | `bool` | `true` when the marker is currently visible on the map (i.e. the player has placed one and not cleared it). |
| `x` | `float \| null` | X coordinate of the marker, in its worldspace's coordinate system. `null` when the marker reference has not been initialized yet (player has never opened the map menu in this save). |
| `y` | `float \| null` | Y coordinate. |
| `z` | `float \| null` | Z coordinate. |
| `worldspace` | `string \| null` | EditorID of the marker's worldspace (in vanilla play this is always `"Tamriel"`). |
| `worldspaceFormId` | `string \| null` | Hex form ID of the marker's worldspace. |
| `parentWorldspace` | `string \| null` | EditorID of the root worldspace (`worldspace`'s `parentWorld` chain root). |
| `parentWorldspaceFormId` | `string \| null` | Hex form ID of the root worldspace. |

> **Note on `isSet=false`:** the spatial fields may still hold the *previous*
> coordinates of the marker (the engine just hides the ref on clear). Treat
> them as undefined when `isSet` is `false`.

### Modifying the marker

Use the `command` message type. Both commands respond with a `commandResult`
whose `data` payload has the same shape as the field above, reflecting the
marker's state **after** the operation:

| Command | Required arguments | Effect |
|---|---|---|
| `player_marker_set` | `x` (number), `y` (number), `z` (number, optional, default `0`) | Places or moves the marker to those coordinates in its current worldspace and makes it visible. Fails if the marker reference is not yet initialized — open the world map at least once in the save first. |
| `player_marker_clear` | — | Hides the marker. Always succeeds. The underlying ref is preserved and will be reused on the next `player_marker_set`. |

Coordinates passed to `player_marker_set` are in the marker's own worldspace.
In vanilla Skyrim that worldspace is the global parent map (Tamriel), so a
client mirroring the in-game map UI should pass Tamriel-space `(x, y)`.

### Example — place the marker

```json
{
  "type": "command",
  "id": "cmd-marker-set",
  "command": "player_marker_set",
  "x": 18000.0,
  "y": -15200.0
}
```

**Server reply:**
```json
{
  "type": "commandResult",
  "id": "cmd-marker-set",
  "success": true,
  "data": {
    "isSet": true,
    "x": 18000.0, "y": -15200.0, "z": 0.0,
    "worldspace": "Tamriel", "worldspaceFormId": "0x0000003C",
    "parentWorldspace": "Tamriel", "parentWorldspaceFormId": "0x0000003C"
  }
}
```

### Example — clear the marker

```json
{
  "type": "command",
  "id": "cmd-marker-clear",
  "command": "player_marker_clear"
}
```

**Server reply:**
```json
{
  "type": "commandResult",
  "id": "cmd-marker-clear",
  "success": true,
  "data": {
    "isSet": false,
    "x": 18000.0, "y": -15200.0, "z": 0.0,
    "worldspace": "Tamriel", "worldspaceFormId": "0x0000003C",
    "parentWorldspace": "Tamriel", "parentWorldspaceFormId": "0x0000003C"
  }
}
```

### Example — query the marker

```json
{
  "type": "query",
  "id": "q-marker",
  "fields": { "marker": "Player::Marker" }
}
```
