# Player Fields Reference

Player fields expose character-level stats that are not available through the
`ActorValue` system: character level, experience points, and inventory weight.

All values are returned as `float` or `integer` as noted.

## Available Player Fields

| Registry key | Value type | Description |
|---|---|---|
| `Player::Level` | `integer` | Character level |
| `Player::XP::Current` | `float` | XP earned within the current level (resets to 0 on level-up) |
| `Player::XP::Next` | `float` | XP threshold required to reach the next level |
| `Player::XP::LevelStart` | `float` | XP value at the start of the current level (always `0.0`; provided for progress-bar math) |
| `Player::InventoryWeight` | `float` | Total weight of all items currently in the player's inventory |
| `Player::CarryWeight` | `float` | Maximum carry weight (same value as `ActorValue::kCarryWeight`) |

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
