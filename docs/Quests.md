# Quests Fields Reference

Quest fields expose the player's quest journal to the client. All quest text
(quest names, objective text) is returned already localized to the game's
current language — the engine's FULL/NNAM records are resolved against the
currently loaded STRINGS file (`sLanguage:General`), so if the game is set to
Russian, the payload will contain Russian strings.

## Available Quest Fields

| Registry key | Value type | Description |
|---|---|---|
| `Quests::Items`          | `array` | Regular (non-miscellaneous) known quests with a full objectives list |
| `Quests::Items::Others`  | `array` | Miscellaneous ("Разное") quests — one-shot tasks without a journal objective list |

Both fields are compatible with the standard `query`, `subscribe`, and
`unsubscribe` message types (see [PROTOCOL.md](../PROTOCOL.md)).

A quest is included in either array when it is currently enabled (running) or
was completed during this playthrough, provided it has a non-empty display
name.

---

## `Quests::Items` Element Shape

```jsonc
{
  "questId":     "0x0001F258",
  "name":        "Освобождение Рассвета",
  "description": "",
  "isActive":    true,
  "isCompleted": false,
  "tasks": [
    { "name": "Найти Изольду",       "isCompleted": true  },
    { "name": "Поговорить с Тайной", "isCompleted": false }
  ]
}
```

- `questId` — hexadecimal FormID of the `TESQuest` record (stable across sessions for quests defined by mods/plugins that keep their load order). Use this value when calling the `set_active_quest` command.
- `name` — quest title as shown in the journal, already localized.
- `description` — currently always `""`. The journal description is stored as a `BGSLocalizedStringDL` reference into the STRINGS table and CommonLibSSE-NG does not expose a resolver for that table, so the description cannot be produced without custom engine code. Preserved in the schema so it can be populated later without a breaking change.
- `isActive` — `true` when this is the HUD/compass-tracked quest (matches the "Сделать активным" / "Make active" journal toggle). At most one quest has `isActive = true` at any time.
- `isCompleted` — `true` once the quest is finished.
- `tasks` — array of the quest's *visible* objectives (dormant/internal stages are hidden). Objective order matches the in-game journal.

### Task element

```jsonc
{ "name": "Найти Изольду", "isCompleted": true }
```

- `name` — localized objective text (NNAM).
- `isCompleted` — `true` when the objective's state is `kCompleted` or `kCompletedDisplayed`. Failed objectives are visible but reported with `isCompleted = false`.

---

## `Quests::Items::Others` Element Shape

Miscellaneous quests (`QUEST_DATA::Type::kMiscellaneous`) don't carry a
per-objective breakdown in the journal, so the shape is flatter:

```jsonc
{
  "isOther":     true,
  "questId":     "0x000CEFC6",
  "name":        "Доставить посылку Фридриху",
  "isActive":    false,
  "isCompleted": true
}
```

- `isOther` — always `true`. Lets clients distinguish these entries from regular quests if the two arrays are merged client-side.
- `questId`, `name`, `isActive`, `isCompleted` — same semantics as in `Quests::Items`.

---

## Commands

### `set_active_quest`

Marks the given quest as the HUD/compass-tracked ("active") quest. This is the
same effect as the player clicking **Сделать активным** / **Make Active** in
the journal. At most one quest is active at any time — the command clears the
active flag on all other quests before setting it on the target.

#### Request

```json
{
  "type": "command",
  "id": "cmd-123",
  "command": "set_active_quest",
  "formId": "0x0001F258"
}
```

- `formId` — the quest's FormID exactly as returned by `questId` in `Quests::Items`.

#### Response

Standard `commandResult` envelope — see [PROTOCOL.md](../PROTOCOL.md#command):

```json
{ "type": "commandResult", "id": "cmd-123", "success": true }
```

On failure, `success` is `false` and `error` contains one of:

- `"Quest not found"` — no quest has that FormID.
- `"Quest is not currently running"` — quest is disabled/unstarted.
- `"Quest is already completed"` — completed quests cannot be made active.
- `"TESDataHandler not available"` — engine state not ready.

---

## Example: subscribing to the journal

```json
{
  "type": "subscribe",
  "id": "journal",
  "settings": { "frequency": 1000, "sendOnChange": true },
  "fields": {
    "main":  "Quests::Items",
    "misc":  "Quests::Items::Others"
  }
}
```

The client will then receive periodic updates under the aliases `main` and
`misc`, in the language currently configured for the game.
