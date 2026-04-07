# WebSocket Protocol — SkyrimWebSocket Plugin

The plugin starts a WebSocket server on `ws://127.0.0.1:8765`.

After the connection is established the client drives all behaviour: it declares
which fields it wants, how often they should be delivered, and can query data
on demand. The server never sends anything until the client sends a message first.

---

## Message format

All messages are UTF-8 JSON objects with a required `"type"` field.

---

## Client → Server messages

### `subscribe`

Starts (or replaces) a repeating push subscription. The server will send a
`"data"` message at the requested frequency for as long as the session is open
or until `"unsubscribe"` is received.

```jsonc
{
  "type": "subscribe",
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
| `settings.frequency` | no | `500` | Push interval in ms. Clamped to minimum 50. |
| `settings.sendOnChange` | no | `false` | Skip the push entirely when no values have changed since the last delivery. |
| `fields` | no | `{}` | Map of user-defined response key → registry key. Empty map produces no data. |

---

### `unsubscribe`

Stops the current push subscription. Sending `"subscribe"` again restarts it.

```json
{ "type": "unsubscribe" }
```

---

### `query`

Performs a **one-shot read** of the requested fields and returns a single
`"data"` message. Does not affect or require an active subscription.

```jsonc
{
  "type": "query",
  "fields": {
    "<alias>": "<registry key>",
    ...
  }
}
```

---

### `describe`

Returns the full list of field keys supported by the plugin, including their
value type and description.

```json
{ "type": "describe" }
```

---

## Server → Client messages

### `data`

Sent in response to a subscription push or a `"query"` request.

```jsonc
{
  "type": "data",
  "ts": 1712462400123,   // Unix timestamp in milliseconds
  "fields": {
    "<alias>": <float>,  // one entry per successfully resolved field
    ...
  }
}
```

### `describe`

Sent in response to a `"describe"` request.

```jsonc
{
  "type": "describe",
  "fields": {
    "ActorValue::kHealth": { "valueType": "float", "description": "Current health points" },
    ...
  }
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

All field values are `float`.

| Registry key | Description |
|---|---|
| `ActorValue::kHealth` | Current health points |
| `ActorValue::kMagicka` | Current magicka points |
| `ActorValue::kStamina` | Current stamina points |
| `ActorValue::kHealRate` | Health regeneration rate |
| `ActorValue::kMagickaRate` | Magicka regeneration rate |
| `ActorValue::kStaminaRate` | Stamina regeneration rate |
| `ActorValue::kHealRateMult` | Health regeneration multiplier |
| `ActorValue::kMagickaRateMult` | Magicka regeneration multiplier |
| `ActorValue::kStaminaRateMult` | Stamina regeneration multiplier |
| `ActorValue::kSpeedMult` | Movement speed multiplier |
| `ActorValue::kCarryWeight` | Carry weight capacity |
| `ActorValue::kAttackDamageMult` | Attack damage multiplier |
| `ActorValue::kCriticalChance` | Critical hit chance |
| `ActorValue::kDamageResist` | Physical damage resistance |
| `ActorValue::kResistMagic` | Magic resistance |
| `ActorValue::kResistFire` | Fire resistance |
| `ActorValue::kResistFrost` | Frost resistance |
| `ActorValue::kResistShock` | Shock resistance |
| `ActorValue::kPoisonResist` | Poison resistance |
| `ActorValue::kOneHanded` | One-Handed skill level |
| `ActorValue::kTwoHanded` | Two-Handed skill level |
| `ActorValue::kArchery` | Archery skill level |
| `ActorValue::kBlock` | Block skill level |
| `ActorValue::kSmithing` | Smithing skill level |
| `ActorValue::kAlchemy` | Alchemy skill level |
| `ActorValue::kEnchanting` | Enchanting skill level |
| `ActorValue::kHeavyArmor` | Heavy Armor skill level |
| `ActorValue::kLightArmor` | Light Armor skill level |
| `ActorValue::kPickpocket` | Pickpocket skill level |
| `ActorValue::kLockpicking` | Lockpicking skill level |
| `ActorValue::kSneak` | Sneak skill level |
| `ActorValue::kSpeech` | Speech skill level |
| `ActorValue::kAlteration` | Alteration skill level |
| `ActorValue::kConjuration` | Conjuration skill level |
| `ActorValue::kDestruction` | Destruction skill level |
| `ActorValue::kIllusion` | Illusion skill level |
| `ActorValue::kRestoration` | Restoration skill level |
| `ActorValue::kDragonSouls` | Collected dragon souls |
| `ActorValue::kShoutRecoveryMult` | Shout recovery multiplier |

Use `{ "type": "describe" }` at runtime to get the full list with descriptions.

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

### Example 4 — Discover available fields

**Client sends:**
```json
{ "type": "describe" }
```

**Server replies:**
```jsonc
{
  "type": "describe",
  "fields": {
    "ActorValue::kHealth":    { "valueType": "float", "description": "Current health points" },
    "ActorValue::kMagicka":   { "valueType": "float", "description": "Current magicka points" },
    // ... all registered fields
  }
}
```
