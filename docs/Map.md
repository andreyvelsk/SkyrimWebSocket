# Map Fields Reference

Map fields expose the player's discovered locations and their metadata.

---

## Available Map Fields

| Registry key | Value type | Description |
|---|---|---|
| `Map::Markers::Locations` | `array` | Map markers visible on the player's world map (discovered locations + pre-set city markers). |
| `Map::Markers::All` | `array` | Same shape as `Map::Markers::Locations` but includes undiscovered/hidden markers in every loaded worldspace. |
| `Map::Markers::Quests` | `array` | Active quest-marker destinations (the floating quest arrows / quest-target icons). |

---

## `Map::Markers::Locations`

Returns an array of every map marker present in the loaded worldspaces with
`isVisible=true` (cities/landmarks pre-set in the ESM, locations the player has
discovered, and markers revealed by quest scripts). Each element is a JSON
object describing one location.

> Use [`Map::Markers::All`](#mapmarkersall) to get **all** markers, including
> undiscovered ones — same entry shape, just unfiltered.

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
    "markers": "Map::Markers::Locations"
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
    "markers": "Map::Markers::Locations"
  }
}
```

---

## `Map::Markers::All`

Identical entry shape to [`Map::Markers::Locations`](#mapmarkerslocations) but
returns **every** map marker in every loaded worldspace, including markers the
player has not yet discovered (`isVisible=false`). Useful for tooling, map
editors, or pre-rendering an offline atlas.

---

## `Map::Markers::Quests`

Returns an array of **active quest-marker destinations** — the markers Skyrim
renders as the floating quest arrows on the compass and as quest-target icons
on the world map. The list mirrors what the player actually sees on the map:

* on SE/AE, only targets present in `PlayerCharacter::PLAYER_RUNTIME_DATA::questTargets`,
  the runtime map Skyrim uses for quest-target candidates,
* only quests whose `TESQuest::IsActive()` / `QuestFlag::kActive` bit is set
  by the journal UI / `SetActiveQuest`,
* for `Miscellaneous` quests, only when the journal's master Miscellaneous
  toggle is enabled,
* only quests that are currently **running** and not completed,
* one entry per visible quest-marker destination. A single objective can still
  produce multiple entries when Skyrim exposes multiple distinct destinations,
  but alternative aliases that resolve to the same marker are collapsed.

In CommonLibSSE-NG, `TESQuest::IsActive()` checks `QuestFlag::kActive`; runtime
testing shows that this bit tracks normal quests marked active through the
journal UI. `questTargets` by itself is broader and can contain displayed but
untracked objectives, especially Miscellaneous objectives. VR currently uses a
best-effort static fallback because its quest-target runtime layout is different.

For coordinate troubleshooting, query `Debug::Map::Markers::Quests` and inspect
`questTargets[].targets[].coordinateDiagnostics`. It lists the selected map
coordinate plus alternative candidates such as the raw target reference,
`TESObjectREFR::GetEditorLocation(out)`, resolved or parent location world/horse
markers, linked teleport doors, and random teleport markers.

Miscellaneous has two layers of tracking in Skyrim's journal: each individual
Misc objective can be active, and the top-level Miscellaneous row has its own
master toggle controlled by the native `ToggleShowMiscObjectives` callback. The
reader observes the top-level Miscellaneous row in the journal Scaleform list
(`Journal_QuestsTab::unk18` first, then `TitleList.entryList` paths, using the
row with `formID == 0`) while the journal menu is open and caches the latest
value after the menu closes. If that UI list is not available, it falls back to
`Journal_QuestsTab::unk30`. Until that UI state has been observed in the current
plugin session, the reader defaults to visible so it does not hide valid Misc
targets unexpectedly.

Targets that resolve to non-ref aliases (location aliases, data aliases) or
to unfilled refs are skipped. References flagged as deleted are still returned
when Skyrim itself keeps them in `questTargets`, because the engine can use
those runtime targets for active quest markers.

`refId` and `name` describe the actual quest target reference (NPC, item, door,
container, etc.). `localX` / `localY` / `localZ` preserve that reference's raw
coordinates inside its current worldspace or interior cell. The spatial fields
(`x`, `y`, `z`, `worldspace`, `cell`, ...) describe where the quest marker
should be drawn on the world map. For targets inside interiors, targets in child
worldspaces, targets without a worldspace, and deleted runtime targets, the
reader walks the target's `BGSLocation` parent hierarchy and uses the nearest
available `BGSLocation::worldLocMarker`. That makes quest marker coordinates
line up with the location markers returned by `Map::Markers::Locations` /
`Map::Markers::All`, instead of leaking local coordinates such as an NPC's
position inside a house.

### Entry shape

| Field | Type | Description |
|---|---|---|
| `questFormId` | `string` | Hex form ID of the quest. |
| `questEditorId` | `string` | Editor ID of the quest (e.g. `"MQ101"`). Empty if not present. |
| `questName` | `string` | Localised quest name. Empty if unnamed. |
| `questType` | `string` | Quest category — one of `MainQuest`, `MagesGuild`, `ThievesGuild`, `DarkBrotherhood`, `Companions`, `Miscellaneous`, `Daedric`, `SideQuest`, `CivilWar`, `DLC01_Vampire`, `DLC02_Dragonborn`, `None`. |
| `isActive` | `bool` | `TESQuest::IsActive()` / `QuestFlag::kActive`. Only active (tracked) quests are returned. |
| `isMiscellaneous` | `bool` | `true` for quests in the Miscellaneous journal section. |
| `miscObjectivesVisible` | `bool` | Present on Miscellaneous entries. Latest observed state of the journal's master Miscellaneous toggle. |
| `miscObjectivesVisibilityKnown` | `bool` | Present on Miscellaneous entries. `true` after the master Miscellaneous toggle has been observed from the journal UI in this plugin session. |
| `miscObjectivesVisibilitySource` | `string` | Present on Miscellaneous entries. Source for the master toggle value (journal Scaleform list, native fallback, cached value, or default). |
| `objectiveIndex` | `integer` | The objective's `QOBJ` index inside the quest. |
| `objectiveText` | `string` | Localised objective description as stored on the quest — may contain unresolved placeholders for radiant/templated quests, e.g. `"<Alias=BanditCamp>: kill the leader"`. |
| `objectiveTextResolved` | `string` | Same text with `<Alias=...>` / `<Alias.ShortName=...>` etc. tokens replaced through the current quest instance data (`aliasName -> aliasID -> fullNameFormID`) when available, e.g. the bandit camp's actual location name. Tokens we can't resolve (unknown aliases, `<Global=...>`, `<Spouse>`, ...) are left untouched. Identical to `objectiveText` when there are no placeholders. |
| `aliasId` | `integer` | The quest alias ID this target points at. |
| `refId` | `string` | Hex form ID of the resolved reference (NPC, door, container, etc.). |
| `isDeleted` | `bool` | `true` when the resolved reference has the form deleted flag set. Some vanilla quest targets still use such refs and are kept if Skyrim exposes them through `questTargets`. |
| `name` | `string` | Display name of the reference. Empty if unnamed. |
| `coordinateSource` | `string` | Source used for `x`/`y`/`z`: `targetRef` for direct target coordinates, `BGSLocation::worldLocMarker` for direct location projection, `BGSLocation::parentLoc.worldLocMarker` when a parent location supplied the map marker, or `targetRef:noLocationMarker` when no map marker could be found. |
| `coordinateRefId` | `string` | Hex form ID of the reference used for the spatial fields. Usually the same as `refId`; for interior targets this is usually the location's map marker reference. |
| `coordinateRefName` | `string` | Display name of `coordinateRefId`, when available. |
| `locationFormId` | `string \| null` | Hex form ID of the `BGSLocation` considered for map-marker projection, or `null` when no location was resolved. |
| `locationEditorId` | `string \| null` | Editor ID of that `BGSLocation`, or `null`. |
| `locationName` | `string \| null` | Localised name of that `BGSLocation`, or `null`. |
| `localX` | `float` | Raw X coordinate of the actual quest target reference. For interior targets this is local to the interior cell. |
| `localY` | `float` | Raw Y coordinate of the actual quest target reference. |
| `localZ` | `float` | Raw Z coordinate of the actual quest target reference. |
| `localWorldspace` | `string \| null` | EditorID of the actual target reference's worldspace. `null` for interior targets. |
| `localWorldspaceFormId` | `string \| null` | Hex form ID of the actual target reference's worldspace. |
| `localParentWorldspace` | `string \| null` | EditorID of the root worldspace for the actual target reference, when it has a worldspace. |
| `localParentWorldspaceFormId` | `string \| null` | Hex form ID of that root worldspace. |
| `localCell` | `string \| null` | EditorID of the actual target reference's parent cell. |
| `localCellFormId` | `string \| null` | Hex form ID of the actual target reference's parent cell. |
| `localIsInterior` | `bool` | `true` if the actual target reference's parent cell is an interior. |
| `x` | `float` | Map-facing X coordinate of the quest marker. For interior targets this is the location world marker coordinate when available. |
| `y` | `float` | Map-facing Y coordinate of the quest marker. |
| `z` | `float` | Map-facing Z coordinate of the quest marker. |
| `worldspace` | `string \| null` | EditorID of the coordinate reference's worldspace. `null` only when no map-facing worldspace could be resolved. |
| `worldspaceFormId` | `string \| null` | Hex form ID of the worldspace. |
| `parentWorldspace` | `string \| null` | EditorID of the root worldspace (walks `parentWorld` to the top, e.g. `"Tamriel"` for city sub-worlds). |
| `parentWorldspaceFormId` | `string \| null` | Hex form ID of the root worldspace. |
| `cell` | `string \| null` | EditorID of the coordinate reference's parent cell. |
| `cellFormId` | `string \| null` | Hex form ID of the cell. |
| `isInterior` | `bool` | `true` if the coordinate reference's parent cell is an interior. |

Use `parentWorldspace` to plot quest markers on a global Tamriel/Solstheim map
(see [`Player::ExteriorPosition`](Player.md) for the same convention applied
to the player). In normal exterior cases `coordinateSource` is `targetRef`. In
interior cases, a successful `BGSLocation::worldLocMarker` projection means the
coordinates already point at the location marker / entrance on the world map.

### Example — query active quest markers

```json
{
  "type": "query",
  "id": "q-quest-markers",
  "fields": {
    "questMarkers": "Map::Markers::Quests"
  }
}
```

**Server reply:**
```json
{
  "type": "data",
  "id": "q-quest-markers",
  "ts": 1712462400500,
  "fields": {
    "questMarkers": [
      {
        "questFormId": "0x0003372B",
        "questEditorId": "MQ102",
        "questName": "Before the Storm",
        "questType": "MainQuest",
        "isActive": true,
        "objectiveIndex": 10,
        "objectiveText": "Talk to the Jarl of Whiterun",
        "objectiveTextResolved": "Talk to the Jarl of Whiterun",
        "aliasId": 0,
        "refId": "0x0001A696",
        "name": "Jarl Balgruuf the Greater",
        "coordinateSource": "BGSLocation::worldLocMarker",
        "coordinateRefId": "0x00018A56",
        "coordinateRefName": "Whiterun",
        "locationFormId": "0x00018A4A",
        "locationEditorId": "WhiterunLocation",
        "locationName": "Whiterun",
        "localX": 128.0, "localY": -512.0, "localZ": 64.0,
        "localWorldspace": null,
        "localWorldspaceFormId": null,
        "localParentWorldspace": null,
        "localParentWorldspaceFormId": null,
        "localCell": "WhiterunDragonsreach",
        "localCellFormId": "0x000165A8",
        "localIsInterior": true,
        "x": 18142.5, "y": -14520.3, "z": 0.0,
        "worldspace": "Tamriel",
        "worldspaceFormId": "0x0000003C",
        "parentWorldspace": "Tamriel",
        "parentWorldspaceFormId": "0x0000003C",
        "cell": "WhiterunExterior01",
        "cellFormId": "0x000095EE",
        "isInterior": false
      }
    ]
  }
}
```

### Example — live subscription

Quest markers can change from quest stages, cell transitions, and direct journal
UI toggles. Unlike location markers, this field is intentionally re-read at the
subscription interval and emitted only when the serialised value changes; that
keeps `sendOnChange` responsive to Journal Menu toggles that do not produce a
stable native event.

```json
{
  "type": "subscribe",
  "id": "quest-markers",
  "settings": { "frequency": 1000, "sendOnChange": true },
  "fields": {
    "quests": "Map::Markers::Quests"
  }
}
```

---

## Commands

### `fast_travel`

Triggers a real, vanilla-style fast travel to a map-marker reference.

| Field | Type | Description |
|---|---|---|
| `formId` | `string` | Hex form ID of the marker reference. Use the `refId` value returned by `Map::Markers::Locations` (e.g. `"0x000136D5"`). |

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

Internally dispatches the Papyrus static `Game.FastTravel(akMarker)` through
the SKSE Virtual Machine. This routes through the engine's full fast-travel
pipeline, exactly like the player confirming a destination on the in-game
map:

* fade animation is played,
* in-game time is advanced based on the travel distance,
* random-encounter rolls are performed,
* weather is reset and autosave is triggered (per the engine's own rules),
* followers are transported with the player,
* the `PlayerFlags::fastTraveling` bit is toggled, so other mods that hook
  it (e.g. `OnFastTravelEnd` listeners) fire correctly.

The command returns success **as soon as the VM call is queued** — actual
arrival happens asynchronously inside the engine. Clients that need to react
to arrival should observe the player's position / current location via the
existing field subscriptions.

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
