# Player Fields Reference

Player fields expose character-level stats that are not available through the
`ActorValue` system: character level, experience points, inventory weight, and
world position.

All values are returned as `float`, `integer`, `object`, or `array` as noted.

## Available Player Fields

| Registry key | Value type | Description |
|---|---|---|
| `Player::Level` | `integer` | Character level |
| `Player::XP::Current` | `float` | XP earned within the current level (resets to 0 on level-up) |
| `Player::XP::Next` | `float` | XP threshold required to reach the next level |
| `Player::XP::LevelStart` | `float` | XP value at the start of the current level (always `0.0`; provided for progress-bar math) |
| `Player::InventoryWeight` | `float` | Total weight of all items currently in the player's inventory |
| `Player::CarryWeight` | `float` | Maximum carry weight (same value as `ActorValue::kCarryWeight`) |
| `Player::Position` | `object` | Player position, heading, worldspace and cell. **`x`/`y`/`z` are always global-map coordinates** — see below |
| `Player::ExteriorPosition` | `object` | **DEPRECATED** — `Player::Position.x/y/z` now always hold global-map coordinates. Kept for backward compatibility — see below |
| `Player::Marker` | `object` | Player-placed custom map marker state (the marker dropped by clicking on the world map) — see below |
| `Player::Quests` | `array` | Current player quest journal entries, including resolved radiant names/objectives, completion state, and Misc quest flags — see below |

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

Returns the player's current position, heading, worldspace, and cell.
**`x`/`y`/`z` are always global-map coordinates** — clients no longer need
[`Player::ExteriorPosition`](#playerexteriorposition) for map placement.

### Object shape

| Field | Type | Description |
|---|---|---|
| `x` | `float` | Global-map X coordinate. For exteriors: live player position. For interiors: `BGSLocation::worldLocMarker` coords (entrance point on the parent world map). |
| `y` | `float` | Global-map Y coordinate. |
| `z` | `float` | Z coordinate (height). |
| `angle` | `float` | Z-axis rotation (yaw / heading) in **radians**. `0` = North, increases clockwise. Convert to degrees: `angle * 180 / π`. |
| `worldspace` | `string \| null` | EditorID of the current worldspace (e.g. `"Tamriel"`, `"WhiterunWorld"`). `null` in interiors. |
| `worldspaceFormId` | `string \| null` | Hex form ID of the current worldspace, or `null` in interiors. |
| `parentWorldspace` | `string \| null` | EditorID of the **root** worldspace (walks `parentWorld` up to the top). For Tamriel-anchored sub-worlds (cities) this equals `"Tamriel"`. `null` in interiors. |
| `parentWorldspaceFormId` | `string \| null` | Hex form ID of the root worldspace. |
| `cell` | `string \| null` | EditorID of the current cell. |
| `cellFormId` | `string \| null` | Hex form ID of the current cell. |
| `isInterior` | `bool` | `true` if the player is inside an interior cell. |

### Coordinate resolution

| Player location | `x`/`y`/`z` source |
|---|---|
| **Exterior, any worldspace** | Live player position |
| **Interior cell** | `BGSLocation::worldLocMarker` — entrance marker on the parent world map (cave door, city gate, etc.). Falls back through cell location → player current location → player editor location chains. |

When no `BGSLocation` marker can be found at all (rare: hand-placed test cells
without location assignment), interior `x`/`y`/`z` fall back to the live cell
coordinates as a last resort.

### Mapping to a global map

Use `x` / `y` directly — they are always in the parent (root) worldspace
coordinate system.

```js
if (pos.parentWorldspace === "Tamriel") {
  plotOnMap(pos.x, pos.y);   // Always correct — even inside interiors.
} else if (pos.parentWorldspace === "DLC2SolstheimWorld") {
  switchToSolstheimMap(pos.x, pos.y);
}
```

### Example — subscription (single field)

```json
{
  "type": "subscribe",
  "id": "map-pos",
  "settings": { "frequency": 100, "sendOnChange": true },
  "fields": { "pos": "Player::Position" }
}
```

**Outside, Tamriel:**
```json
{
  "type": "data", "id": "map-pos", "ts": 1712462400123,
  "fields": {
    "pos": {
      "x": -89010.55, "y": -26747.70, "z": 312.0, "angle": 1.57,
      "worldspace": "Tamriel", "worldspaceFormId": "0x0000003C",
      "parentWorldspace": "Tamriel", "parentWorldspaceFormId": "0x0000003C",
      "cell": "Wilderness", "cellFormId": "0x00009BE9",
      "isInterior": false
    }
  }
}
```

**Inside an interior (same location — entrance marker coords):**
```json
{
  "type": "data", "id": "map-pos", "ts": 1712462400456,
  "fields": {
    "pos": {
      "x": -89010.55, "y": -26747.70, "z": 0.0, "angle": 0.78,
      "worldspace": null, "worldspaceFormId": null,
      "parentWorldspace": "Tamriel", "parentWorldspaceFormId": "0x0000003C",
      "cell": "SomeCave01", "cellFormId": "0x0001A273",
      "isInterior": true
    }
  }
}
```

---

## `Player::ExteriorPosition`

> **DEPRECATED** — [`Player::Position`](#playerposition) now returns global-map
> coordinates directly in `x`/`y`/`z`, including for interiors.
> `Player::ExteriorPosition` is kept for backward compatibility but will be
> removed in a future version.

Returns the position to use when rendering the player on the global world map.

- **In a top-level exterior worldspace** (Tamriel, Solstheim, …) — live player
  coordinates and that worldspace.  No caching, always up-to-date.
- **In an interior cell or a city sub-worldspace** (cave, dungeon, inn, Whiterun,
  …) — the fixed coordinates of the location's map marker (`BGSLocation::worldLocMarker`),
  i.e. the entrance point visible on the world map (e.g. the dungeon door on Tamriel,
  or the city gate marker).  The location hierarchy is walked upward until a marker is
  found, so even deeply nested locations are covered.

### Object shape

| Field | Type | Description |
|---|---|---|
| `x` | `float \| null` | Map X coordinate. |
| `y` | `float \| null` | Map Y coordinate. |
| `z` | `float \| null` | Map Z coordinate. |
| `worldspace` | `string \| null` | EditorID of the worldspace. |
| `worldspaceFormId` | `string \| null` | Hex form ID of the worldspace. |
| `parentWorldspace` | `string \| null` | EditorID of the root worldspace (see `Player::Position`). |
| `parentWorldspaceFormId` | `string \| null` | Hex form ID of the root worldspace. |

All fields are `null` only when the player is in a cell that has no `BGSLocation`
assignment at all (very rare; typically hand-placed test cells).

---

## `Player::Quests`

Returns the current player-available quest journal entries. The reader includes
running, non-completed quests that have visible journal objectives, a quest log
entry, or an active/tracked state with a usable display name, which filters out
most hidden technical quests.

Radiant quest placeholders such as `<Alias=Location>` are resolved with the
same alias/instance-data path used by `Map::Markers::Quests`, so generated quest
names and objective text use the current instance's NPC/place/item names when
the engine exposes them.

### Quest object shape

| Field | Type | Description |
|---|---|---|
| `questFormId` | `string` | Hex form ID of the `TESQuest`. Use this as `formId` for `quest_set_active`. |
| `questEditorId` | `string` | EditorID when available. |
| `name` | `string` | Resolved display name. For Misc quests with no quest title, falls back to the first visible objective text. |
| `nameRaw` | `string` | Raw quest full name before radiant alias replacement. |
| `description` | `string` | Latest resolved quest journal log entry observed in the player's quest log. Empty when the engine has no readable log text for the quest. |
| `descriptionRaw` | `string` | Raw log text before radiant alias replacement. |
| `descriptionStage` | `integer` | Stage index that provided `description`, or `0` when no log entry was available. |
| `type` / `questType` | `string` | Quest category, e.g. `MainQuest`, `SideQuest`, `Miscellaneous`. Both keys carry the same stable value. |
| `isMisc` | `bool` | `true` for quests in the Miscellaneous journal section. |
| `isActive` | `bool` | `true` when the quest is currently tracked/active (`QuestFlag::kActive`). |
| `isRunning` | `bool` | `true` for running quests. Included for client diagnostics. |
| `isCompleted` | `bool` | `true` for completed quests. The normal list excludes completed quests. |
| `currentStage` | `integer` | Current quest stage ID. |
| `currentInstanceId` | `integer` | Current radiant quest instance ID when present. |
| `steps` | `array` | Ordered visible quest objectives — see below. |
| `miscMarkersVisible` | `bool` | Misc quests only. Current shared Misc marker filter used by `Map::Markers::Quests`. |
| `miscMarkersVisibilityKnown` | `bool` | Misc quests only. `true` after the plugin has observed or explicitly set the Misc master marker state. |
| `miscMarkersVisibilitySource` | `string` | Misc quests only. Source of the shared Misc marker state. |

### Step object shape

| Field | Type | Description |
|---|---|---|
| `index` | `integer` | Quest objective index. Steps are sorted by this value. |
| `text` | `string` | Resolved objective text. |
| `textRaw` | `string` | Raw objective text before radiant alias replacement. |
| `completed` | `bool` | `true` when the objective state is completed. |
| `failed` | `bool` | `true` when the objective state is failed. |
| `state` | `string` | Raw Skyrim objective state name, e.g. `Displayed`, `CompletedDisplayed`, `FailedDisplayed`. |
| `stateRaw` | `integer` | Numeric objective state. |
| `instanceId` | `integer` | Radiant instance ID used for alias text resolution, or `0` when unavailable. |

### Example — query current quests

```json
{
  "type": "query",
  "id": "q-quests",
  "fields": { "quests": "Player::Quests" }
}
```

**Server reply:**
```json
{
  "type": "data",
  "id": "q-quests",
  "ts": 1712462400123,
  "fields": {
    "quests": [
      {
        "questFormId": "0x00036192",
        "questEditorId": "MQ102",
        "name": "Before the Storm",
        "description": "Gerdur told me to travel to Whiterun and speak to the Jarl.",
        "questType": "MainQuest",
        "isMisc": false,
        "isActive": true,
        "steps": [
          {
            "index": 30,
            "text": "Talk to the Jarl of Whiterun",
            "completed": false,
            "failed": false,
            "state": "Displayed"
          }
        ]
      }
    ]
  }
}
```

### Modifying quest tracking

Use `quest_set_active` to set or clear the active/tracked marker on a single
quest. The response `data` payload is the updated `Player::Quests` entry shape
for that quest.

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
