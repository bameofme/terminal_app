# Recipe Format

TCM recipes are JSON files that automate expect/send interactions with a connected device. Place recipe files in `~/.config/tcm/recipes/` or load them from any path via the TUI.

---

## Top-level Structure

```json
{
  "name":        "<string>",
  "description": "<string>",
  "steps":       [ <step>, ... ],
  "variables":   { "<key>": "<default_value>", ... }
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | yes | Short identifier for the recipe |
| `description` | string | no | Human-readable description |
| `steps` | array | yes | Ordered list of expect/send steps |
| `variables` | object | no | Named variables with default values |

---

## Step Object

```json
{
  "expect":     "<regex>",
  "send":       "<string>",
  "timeout_ms": <integer>,
  "on_timeout": "abort" | "skip" | "continue"
}
```

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `expect` | string | yes | — | POSIX extended regex to wait for in the device output |
| `send` | string | yes | — | String to send once the pattern is matched. Supports variable substitution. |
| `timeout_ms` | integer | no | `5000` | Milliseconds to wait for the `expect` pattern before triggering `on_timeout` |
| `on_timeout` | string | no | `"abort"` | Action when timeout expires: `abort`, `skip`, or `continue` |

### `on_timeout` values

| Value | Behaviour |
|-------|-----------|
| `abort` | Stop recipe execution and report failure |
| `skip` | Skip the `send` for this step and advance to the next step |
| `continue` | Send the string anyway and advance to the next step |

---

## Variables

Variables are declared in the top-level `variables` object with optional default values. At runtime, TCM prompts for any variable whose value is empty (`""`).

Reference a variable inside a `send` string using `{variable_name}`:

```json
"send": "{username}\n"
```

### Example

```json
"variables": {
  "username": "admin",
  "password": "",
  "enable_secret": ""
}
```

- `username` defaults to `"admin"` — TCM will not prompt for it unless overridden
- `password` and `enable_secret` are empty — TCM will prompt the user at recipe start

---

## Regex Syntax

The `expect` field uses POSIX Extended Regular Expressions (ERE). Common patterns:

| Pattern | Matches |
|---------|---------|
| `(?i)login` | Case-insensitive "login" |
| `(?i)password` | Case-insensitive "password" |
| `[#$>]` | Any of `#`, `$`, `>` (typical shell/router prompts) |
| `#` | Literal `#` (privilege mode prompt) |
| `Username:` | Literal string |
| `\(config\)#` | Cisco config mode prompt |

> **Note**: TCM searches the accumulated terminal output since the last `send`. The match is not anchored — any substring match succeeds.

---

## Special Characters in `send`

| Sequence | Description |
|----------|-------------|
| `\n` | Newline (Enter key) |
| `\r` | Carriage return |
| `\t` | Tab |
| `{var}` | Variable substitution |

---

## Complete Example

```json
{
  "name": "cisco-enable-login",
  "description": "Login to Cisco IOS device and enter enable mode",
  "steps": [
    {
      "expect": "(?i)username",
      "send": "{username}\n",
      "timeout_ms": 10000,
      "on_timeout": "abort"
    },
    {
      "expect": "(?i)password",
      "send": "{password}\n",
      "timeout_ms": 5000,
      "on_timeout": "abort"
    },
    {
      "expect": ">",
      "send": "enable\n",
      "timeout_ms": 3000,
      "on_timeout": "abort"
    },
    {
      "expect": "(?i)password",
      "send": "{enable_secret}\n",
      "timeout_ms": 5000,
      "on_timeout": "abort"
    },
    {
      "expect": "#",
      "send": "terminal length 0\n",
      "timeout_ms": 3000,
      "on_timeout": "skip"
    },
    {
      "expect": "#",
      "send": "show version\n",
      "timeout_ms": 5000,
      "on_timeout": "skip"
    }
  ],
  "variables": {
    "username": "admin",
    "password": "",
    "enable_secret": ""
  }
}
```

---

## File Location

TCM searches for recipes in the following locations (in order):

1. `~/.config/tcm/recipes/`
2. The path specified when loading manually from the TUI

Recipe files must have the `.json` extension to be listed in the TUI picker.

---

## Bundled Example Recipes

| File | Description |
|------|-------------|
| `ont-login.json` | Login to ONT broadband device via Telnet |
| `switch-login.json` | Login to managed switch via Telnet |
| `router-login.json` | SSH login to router and show interface summary |
