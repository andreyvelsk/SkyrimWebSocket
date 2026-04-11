# Game Fields Reference

Game fields expose engine-level settings that apply to the running game instance rather
than to the player character. All values are returned as `string`.

## Available Game Fields

| Registry key | Value type | Description |
|---|---|---|
| `Game::Language` | `string` | Current game language read from the `sLanguage:General` INI setting |

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
