# Magic Fields Reference

This document describes the `Magic::*` API and related fields.

## Registry keys

| Registry key | Value type | Description |
|---|---|---|
| `Magic::Categories` | `array` | Non-empty magic categories with spell counts |
| `Magic::Items::All` | `array` | All spells known to the player |
| `Magic::Items::Destruction` | `array` | Destruction spells in player spellbook |
| `Magic::Items::Alteration` | `array` | Alteration spells in player spellbook |
| `Magic::Items::Conjuration` | `array` | Conjuration spells in player spellbook |
| `Magic::Items::Illusion` | `array` | Illusion spells in player spellbook |
| `Magic::Items::Restoration` | `array` | Restoration spells in player spellbook |
| `Magic::Items::Favorites` | `array` | Favorited spells (presently empty; reserved) |

---

## `Magic::Items::*` element shape

```jsonc
{
  "name": "Fireball",
  "formId": "0x00012ABC",
  "effects": [ { "name": "Damage", "magnitude": 10.0, "duration": 0, "descriptionTemplate": "Do <mag> points of fire damage.", "description": "Do 10 points of fire damage." } ]
}
```

- `name` — localized spell name
- `formId` — unique form identifier as hex string
- `effects` — array of effect objects; each has `name`, `magnitude`, `duration`, `descriptionTemplate`, and `description`

Notes:
- The `descriptionTemplate` field is copied verbatim from the EffectSetting's
  DNAM; it may contain placeholders `<mag>` and `<dur>` which the client can
  substitute for presentation.
- The `description` field in this initial implementation is not fully
  resolved and mirrors `descriptionTemplate`.  Clients may substitute values
  for `<mag>` and `<dur>` when rendering.
- Per-school classification (`Magic::Items::Destruction`, etc.) is performed
  using a heuristic that inspects effect and spell names. This is a best-effort
  fallback and may produce incorrect results for non-English game localizations
  or uncommon effects. Future work can replace the heuristic with direct
  engine metadata when available.

## Equipping and favorites

- `GameWriter::EquipSpell(formId, hand)` — stub present in the API. On this
  runtime the operation is not implemented and returns an error.  Implementation
  would need to call into the actor equip/casting binding APIs to attach a
  spell to `right`, `left`, or `both` casting slots.
- `GameWriter::UnequipSpell(formId, hand)` — stub, not implemented.
- `GameWriter::FavoriteSpell(formId)` — returns `not supported` because the
  base game does not maintain a separate favorited state for spells.

## Implementation notes for future work

- Detection of known spells uses `PlayerCharacter::HasSpell` where available.
  If the runtime's symbol names or APIs differ, adapt the checks to the local
  SKSE/CommonLibSSE version.
- Favoriting spells can be implemented by observing the game's quickslot
  binding system (hotkeys) or by maintaining a separate persisted favorites
  registry in the plugin.
- Equipping spells to hands requires using the game's equip/binding APIs
  (ActorEquipManager or internal spell-binding functions). The current code
  includes conservative stubs that return clear errors to avoid unsafe calls.
