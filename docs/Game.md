# Game Fields Reference

Game fields expose engine-level settings that apply to the running game instance rather
than to the player character.

## Available Game Fields

| Registry key | Value type | Description |
|---|---|---|
| `Game::Language` | `string` | Current game language read from the `sLanguage:General` INI setting |
| `Game::Status` | `object` | Runtime state flags telling whether the player can perform actions right now |

---

## `Game::Language`

Returns the language code exactly as stored in `Skyrim.ini` (or the active override INI).
The value is set at game launch and does not change at runtime.

### Possible values

| Value | Language |
|---|---|
| `english` | English |
| `french` | French |
| `german` | German |
| `italian` | Italian |
| `japanese` | Japanese |
| `korean` | Korean |
| `polish` | Polish |
| `portuguese` | Brazilian Portuguese |
| `russian` | Russian |
| `spanish` | Spanish (Spain) |
| `spanish_mexico` | Spanish (Latin America) |
| `chinese` | Simplified Chinese |
| `tchinese` | Traditional Chinese |
| `czech` | Czech |
| `hungarian` | Hungarian |
| `romanian` | Romanian |
| `turkish` | Turkish |

> **Note:** The fallback value when the setting cannot be read is `"english"`.
> If you have installed a community-translated version of Skyrim the value may differ
> from the list above; in that case the raw INI string is returned as-is.

### Example — one-shot language query

**Client sends:**
```json
{
  "type": "query",
  "id": "lang-check",
  "fields": {
    "language": "Game::Language"
  }
}
```

**Server replies (English installation):**
```json
{
  "type": "data",
  "id": "lang-check",
  "ts": 1712462400123,
  "fields": {
    "language": "english"
  }
}
```

**Server replies (Russian installation):**
```json
{
  "type": "data",
  "id": "lang-check",
  "ts": 1712462400123,
  "fields": {
    "language": "russian"
  }
}
```

---

## `Game::Status`

Returns a JSON **object** describing whether the player is currently able to perform
actions, and *why* they cannot if they are blocked. Designed to be subscribed to
once with `sendOnChange: true` so the client gets a push whenever any of the
flags flip (loading screen appears, dialogue starts/ends, combat begins, etc.).

### Object schema

| Field | Type | Description |
|---|---|---|
| `paused`           | `boolean` | `true` while the engine has the world frozen — i.e. any pause-game menu is on the menu stack (Inventory / Magic / Map / Journal / Console / system pause / **crafting menus** like Forge / Alchemy / Enchanting). Backed by `RE::UI::GameIsPaused()`. |
| `loading`          | `boolean` | `true` while a **loading screen / cell transition / fast travel / save load** is in progress. Combines the `Loading Menu` being open with the engine flag `PlayerCharacter::playerFlags.isLoading` (the latter is set during interior↔exterior door transitions where no `LoadingMenu` is shown). |
| `inMainMenu`       | `boolean` | `true` while the title screen / main menu is active (no save loaded yet, or the player exited to the main menu). |
| `inDialogue`       | `boolean` | `true` while the player is engaged in a dialogue / conversation with an NPC. Combines the `Dialogue Menu` being open with `MenuTopicManager::speaker` / `lastSpeaker` (the latter stays set briefly after the menu closes while the NPC finishes speaking). |
| `inCombat`         | `boolean` | `true` while the player is in combat. Useful informational flag — combat does **not** by itself prevent actions, so it is not factored into `canAct`. |
| `dead`             | `boolean` | `true` whenever the player's `ActorState::lifeState` is anything other than `kAlive` — covers the dying animation, ragdoll, "you died" load-screen, bleedout (Essential), reanimate / restrained states. |
| `controlsEnabled`  | `boolean` | `true` only when the engine is letting the player control the character. Goes `false` if **any** of: `PlayerControls::blockPlayerInput` is set (cinematic / scripted scene / cart intro), `Actor::IsInKillMove()` is true (killmove animation), the player is occupying a furniture (workbench / forge / alchemy / cooking pot / chair / bed — `Actor::GetOccupiedFurniture()` returns a valid handle), the actor is in a non-`kNormal` sit / sleep / mount transition (`SIT_SLEEP_STATE`), the actor is knocked down / staggered (`KNOCK_STATE_ENUM != kNormal`), or the player is dead. |
| `canAct`           | `boolean` | Convenience flag — `true` only when **all** of the following hold: not `paused`, not `loading`, not `inMainMenu`, not `inDialogue`, not `dead`, and `controlsEnabled` is `true`. Use this when you just want to know "can my external tool send a hotkey / equip request right now?". |

> **Note on `canAct`:** `inCombat` is intentionally *not* part of `canAct`. The
> player can still press buttons, drink potions, swap spells, etc. while
> fighting. If your tool also wants to refuse actions during combat, AND the
> `inCombat` flag in your client.

### Typical scenarios — what each scenario looks like

| Scenario | `paused` | `loading` | `inMainMenu` | `inDialogue` | `inCombat` | `dead` | `controlsEnabled` | `canAct` |
|---|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|
| Free roaming, exploring                | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ |
| In combat                              | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ✅ | ✅ |
| Pause menu / Inventory open            | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| Cell transition (door)                 | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| Fast-travel loading screen             | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ |
| Conversation with NPC                  | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ | ✅ | ❌ |
| Cinematic / killmove / cart ride       | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| At workbench / forge / alchemy / etc.  | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Cooking-pot stir animation             | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Sitting on a chair / sleeping          | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Knocked down / staggered / ragdoll     | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| Player dying animation                 | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| "You died" load screen                 | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ | ❌ | ❌ |
| Title screen, no save loaded           | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ | ✅/❌ | ❌ |

> The pause menu also pauses the game, so `paused` is `true` whenever a
> pause-game UI menu is open. Loading screens technically pause the world too,
> hence the `paused: true` for the cell-transition row.

### Example — subscribing to status

**Client sends:**
```json
{
  "type": "subscribe",
  "id": "status-watch",
  "settings": { "sendOnChange": true },
  "fields": {
    "status": "Game::Status"
  }
}
```

**Server pushes when nothing is happening:**
```json
{
  "type": "data",
  "id": "status-watch",
  "ts": 1712462400123,
  "fields": {
    "status": {
      "paused":          false,
      "loading":         false,
      "inMainMenu":      false,
      "inDialogue":      false,
      "inCombat":        false,
      "dead":            false,
      "controlsEnabled": true,
      "canAct":          true
    }
  }
}
```

**Server pushes when player opens inventory:**
```json
{
  "type": "data",
  "id": "status-watch",
  "ts": 1712462400500,
  "fields": {
    "status": {
      "paused":          true,
      "loading":         false,
      "inMainMenu":      false,
      "inDialogue":      false,
      "inCombat":        false,
      "dead":            false,
      "controlsEnabled": true,
      "canAct":          false
    }
  }
}
```

**Server pushes during a loading screen:**
```json
{
  "type": "data",
  "id": "status-watch",
  "ts": 1712462401200,
  "fields": {
    "status": {
      "paused":          true,
      "loading":         true,
      "inMainMenu":      false,
      "inDialogue":      false,
      "inCombat":        false,
      "dead":            false,
      "controlsEnabled": true,
      "canAct":          false
    }
  }
}
```

### Recommended usage pattern

```text
if (status.canAct) {
    // safe to send equip / hotkey / cast / use-item commands
} else if (status.dead) {
    // player died — wait for respawn / load
} else if (status.loading) {
    // show "loading…" indicator, queue or drop the request
} else if (status.inDialogue) {
    // wait for dialogue to end before retrying
} else if (status.paused) {
    // game UI is open — usually safe to wait and retry
} else if (!status.controlsEnabled) {
    // cinematic / crafting / killmove / scripted scene — do nothing until it ends
}
```

**Server replies (Japanese installation):**
```json
{
  "type": "data",
  "id": "lang-check",
  "ts": 1712462400123,
  "fields": {
    "language": "japanese"
  }
}
```

**Server replies (setting unavailable — fallback):**
```json
{
  "type": "data",
  "id": "lang-check",
  "ts": 1712462400123,
  "fields": {
    "language": "english"
  }
}
```
