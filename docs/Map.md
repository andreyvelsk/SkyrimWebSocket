# Map Fields Reference

Map fields expose the player's discovered locations and their metadata.

---

## Available Map Fields

| Registry key | Value type | Description |
|---|---|---|
| `Map::Markers` | `array` | All map markers in every loaded worldspace (discovered and undiscovered) |

---

## `Map::Markers`

Returns an array of every map marker present in the loaded worldspaces. Each
element is a JSON object describing one location. Use the `isVisible` flag to
distinguish markers the player has already discovered from those that are still
hidden on the in-game map.

### Entry shape

| Field | Type | Description |
|---|---|---|
| `refId` | `string` | Hex form ID of the marker reference (e.g. `"0x000136D5"`). Can be used as a fast-travel target. |
| `name` | `string` | Localised location name (e.g. `"Whiterun"`, `"Bleak Falls Barrow"`). Empty string if unnamed. |
| `type` | `string` | Location type name — see [Marker types](#marker-types) below. |
| `typeId` | `integer` | Numeric type ID. Matches the index in the type table below. |
| `x` | `float` | World X coordinate of the marker. |
| `y` | `float` | World Y coordinate of the marker. |
| `isVisible` | `bool` | `true` if the marker is currently visible on the in-game map. |
| `canFastTravel` | `bool` | `true` if the location can be fast-travelled to. |

### Marker types

| `typeId` | `type` | Description |
|---|---|---|
| 0 | `None` | No type assigned |
| 1 | `City` | Major city (e.g. Whiterun, Solitude) |
| 2 | `Town` | Small town (e.g. Riverwood, Rorikstead) |
| 3 | `Settlement` | Minor settlement |
| 4 | `Cave` | Cave or underground location |
| 5 | `Camp` | Bandit camp or outdoor camp |
| 6 | `Fort` | Fort or military stronghold |
| 7 | `NordicRuin` | Ancient Nordic ruin |
| 8 | `DwemerRuin` | Dwemer (Dwarf) ruin |
| 9 | `Shipwreck` | Shipwreck |
| 10 | `Grove` | Grove or woodland area |
| 11 | `Landmark` | General landmark |
| 12 | `DragonLair` | Dragon lair |
| 13 | `Farm` | Farm |
| 14 | `WoodMill` | Wood mill |
| 15 | `Mine` | Mine |
| 16 | `ImperialCamp` | Imperial military camp |
| 17 | `StormcloakCamp` | Stormcloak military camp |
| 18 | `Doomstone` | Standing stone |
| 19 | `WheatMill` | Wheat mill |
| 20 | `Smelter` | Smelter |
| 21 | `Stable` | Stable |
| 22 | `ImperialTower` | Imperial watchtower |
| 23 | `Clearing` | Clearing |
| 24 | `Pass` | Mountain pass |
| 25 | `Altar` | Altar |
| 26 | `Rock` | Rock formation |
| 27 | `Lighthouse` | Lighthouse |
| 28 | `OrcStronghold` | Orc stronghold |
| 29 | `GiantCamp` | Giant camp |
| 30 | `Shack` | Shack |
| 31 | `NordicTower` | Nordic tower |
| 32 | `NordicDwelling` | Nordic dwelling |
| 33 | `Docks` | Docks / harbour |
| 34 | `Shrine` | Shrine |
| 35 | `RiftenCastle` | Riften — castle |
| 36 | `RiftenCapitol` | Riften — city hall |
| 37 | `WindhelmCastle` | Windhelm — castle |
| 38 | `WindhelmCapitol` | Windhelm — city hall |
| 39 | `WhiterunCastle` | Whiterun — castle |
| 40 | `WhiterunCapitol` | Whiterun — city hall |
| 41 | `SolitudeCastle` | Solitude — castle |
| 42 | `SolitudeCapitol` | Solitude — city hall |
| 43 | `MarkarthCastle` | Markarth — castle |
| 44 | `MarkarthCapitol` | Markarth — city hall |
| 45 | `WinterholdCastle` | Winterhold — castle |
| 46 | `WinterholdCapitol` | Winterhold — city hall |
| 47 | `MorthalCastle` | Morthal — castle |
| 48 | `MorthalCapitol` | Morthal — city hall |
| 49 | `FalkreathCastle` | Falkreath — castle |
| 50 | `FalkreathCapitol` | Falkreath — city hall |
| 51 | `DawnstarCastle` | Dawnstar — castle |
| 52 | `DawnstarCapitol` | Dawnstar — city hall |
| 53 | `DLC02MiraakTemple` | DLC: Dragonborn — Temple of Miraak |
| 54 | `DLC02RavenRock` | DLC: Dragonborn — Raven Rock |
| 55 | `DLC02BeastStone` | DLC: Dragonborn — Beast Stone |
| 56 | `DLC02TelMithryn` | DLC: Dragonborn — Tel Mithryn |
| 57 | `DLC02ToSkyrim` | DLC: Dragonborn — passage to Skyrim |
| 58 | `DLC02StalhrimSource` | DLC: Dragonborn — Stalhrim source |
| 59 | `DLC02CastleKarstaag` | DLC: Dragonborn — Castle Karstaag |
| 60 | `Unknown` | Internal sentinel (`kTotalLocationTypes`) |
| 61 | `Door` | Door / interior entrance marker |
| 62 | `QuestTarget` | Active quest target |
| 63 | `Unknown` | Reserved |
| 64 | `PlayerSet` | Player-placed custom marker |
| 65 | `YouAreHere` | "You Are Here" marker |

### Example — one-shot query for all discovered locations

```json
{
  "type": "query",
  "id": "map-load",
  "fields": {
    "markers": "Map::Markers"
  }
}
```

**Server reply:**
```json
{
  "type": "data",
  "id": "map-load",
  "ts": 1712462400123,
  "fields": {
    "markers": [
      {
        "refId": "0x000136D5",
        "name": "Whiterun",
        "type": "City",
        "typeId": 1,
        "x": 18142.5,
        "y": -14520.3,
        "isVisible": true,
        "canFastTravel": true
      },
      {
        "refId": "0x0002C963",
        "name": "Bleak Falls Barrow",
        "type": "NordicRuin",
        "typeId": 7,
        "x": 10521.0,
        "y": -9403.0,
        "isVisible": true,
        "canFastTravel": true
      }
    ]
  }
}
```

### Example — live subscription (updated every 30 s, change-only)

Useful when the player is actively exploring and new markers may appear:

```json
{
  "type": "subscribe",
  "id": "map-markers",
  "settings": { "frequency": 30000, "sendOnChange": true },
  "fields": {
    "markers": "Map::Markers"
  }
}
```

---

## Commands

### `fast_travel`

Teleports the player to a map-marker reference.

| Field | Type | Description |
|---|---|---|
| `formId` | `string` | Hex form ID of the marker reference. Use the `refId` value returned by `Map::Markers` (e.g. `"0x000136D5"`). |

#### Pre-flight checks

The command fails (with `success: false` and a human-readable `error`) when any
of the following are not satisfied:

* The form ID resolves to an actual `TESObjectREFR`.
* The reference carries an `ExtraMapMarker` with `MapMarkerData` (i.e. it is a
  map marker, not just any reference).
* The marker has been **discovered** (`isVisible == true`).
* The marker has `canFastTravel == true`.
* The reference is not disabled or deleted.
* The marker's parent worldspace does not have the `kCantFastTravel` flag.
* The player is not currently in combat.

#### Behaviour

Internally executes `player.moveto <refId>` via `Script::CompileAndRun`. This
works **cross-worldspace** (you can call it from inside a city sub-world or an
interior cell) and routes through the engine's normal teleport pipeline.

> **Note:** like the `player.moveto` console command, this teleport does not
> advance the in-game clock and does not play the fast-travel fade animation.
> If you need vanilla-style time-passing fast travel, advance the clock with
> a separate command after the teleport.

#### Request

```json
{
  "type": "command",
  "id": "ft-1",
  "command": "fast_travel",
  "formId": "0x000136D5"
}
```

#### Successful response

```json
{
  "type": "commandResult",
  "id": "ft-1",
  "success": true,
  "data": {
    "refId": "0x000136D5",
    "name": "Whiterun Stables",
    "typeId": 21,
    "x": 1185.0,
    "y": -3970.0,
    "isVisible": true,
    "canFastTravel": true
  }
}
```

#### Error response

```json
{
  "type": "commandResult",
  "id": "ft-1",
  "success": false,
  "error": "Marker is flagged as non-fast-travel (canFastTravel=false)"
}
```
