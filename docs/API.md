# Lua Bridge for OBS: script API (api_version 1)

This is the contract between the plugin and scripts. The plugin exposes **procedures** that scripts call, and emits **signals** that scripts connect to. All structured data is passed as UTF-8 JSON strings.

Scripts must keep working without the plugin. Call `luabridge_get_info` first. If the call fails (the procedure doesn't exist), the plugin isn't installed, so skip everything else, including connecting to the `luabridge_*` signals.

## Calling a procedure from Lua

```lua
local cd = obslua.calldata_create()
obslua.calldata_set_string(cd, "owner", "stopwatch")
obslua.calldata_set_string(cd, "json", '{"display_name":"Stopwatch"}')
local found = obslua.proc_handler_call(obslua.obs_get_proc_handler(), "luabridge_register", cd)
local ok = found and obslua.calldata_bool(cd, "ok")
local err = obslua.calldata_string(cd, "error")
obslua.calldata_destroy(cd)
```

`found` is false when the plugin isn't installed.

## Procedures

All procedures are on the global proc handler and set `out bool ok` and `out string error`. On success, `error` is an empty string. On failure, `ok` is false, `error` holds a short message, and the plugin logs a warning prefixed with `[lua-bridge]`. No procedure crashes or throws, whatever its input.

| Procedure | Inputs | Extra outputs | Purpose |
|---|---|---|---|
| `luabridge_get_info` | — | `string json` | Plugin info (see below). Use this to detect the plugin. |
| `luabridge_register` | `string owner`, `string json` | — | Declare the display name, commands and dock controls. Registering again replaces the previous registration. |
| `luabridge_unregister` | `string owner` | — | Remove everything for this owner. Call it from `script_unload`. Always succeeds for a valid owner ID, even one that isn't registered. |
| `luabridge_set_state` | `string owner`, `string json` | — | Merge key/values into the owner's state. |
| `luabridge_emit` | `string owner`, `string event`, `string json` | — | Send a custom event to other scripts (`luabridge_event`) and, from M2, to websocket clients. |
| `luabridge_heartbeat` | `string owner` | — | Optional liveness ping (see Heartbeat). |
| `luabridge_run_command` | `string owner`, `string command`, `string json` | — | Invoke another owner's command. The target receives `luabridge_command` with `origin = "script"`. |

`luabridge_run_command` is an addition to spec B5, which has no way for a script to invoke a command. It's what lets B6's `origin = "script"` happen.

### `luabridge_get_info` JSON

```json
{"api_version":1,"plugin_version":"0.1.0","obs_version":"32.2.2","capabilities":["commands","state","events","heartbeat","run_command"]}
```

## Signals

All signals are on the global signal handler and declared when the plugin loads, before any script runs.

| Signal | Parameters | Sent when |
|---|---|---|
| `luabridge_command` | `string owner, string command, string json, string origin` | A command is invoked for `owner`. `origin` is `dock`, `websocket` or `script`. `json` is always an object (`{}` when there are no args). |
| `luabridge_event` | `string owner, string event, string json` | Any owner calls `luabridge_emit`. |
| `luabridge_ready` | `string json` | After the plugin finishes loading, and again when the OBS frontend finishes loading. `json` is the same as `luabridge_get_info`. |

**Threading and order:**
- Signals are always delivered **asynchronously** on the OBS UI thread, **after the call that caused them returns**, in **FIFO order**. This holds whichever thread the call came from: `script_load` and dock callbacks (UI thread), `timer_add` callbacks (graphics thread), or obs-websocket requests.
- A script that calls `luabridge_run_command` or `luabridge_emit` gets `ok` back first. Its own handler never runs inside the call.
- No signals are sent once OBS starts shutting down (`OBS_FRONTEND_EVENT_EXIT`). Signals still queued at that point are dropped.
- After the plugin has been unloaded, procedures return immediately without logging. `luabridge_unregister` returns `ok = true`, because the owner is already gone, so calling it from `script_unload` never fails on exit. Every other procedure returns `ok = false` with `error = "plugin unloaded"`.

Every script connected to `luabridge_command` receives every command, so filter on `owner`.

## Registration JSON

```json
{
  "display_name": "Stopwatch",
  "commands": [
    { "id": "start", "label": "Start", "description": "Start the timer" },
    { "id": "reset", "label": "Reset", "confirm": true },
    { "id": "add",   "label": "+",     "args": { "seconds": "int" } }
  ],
  "dock": [
    { "type": "label",  "bind": "display", "style": "large" },
    { "type": "row",    "items": [ { "type": "button", "command": "start" } ] },
    { "type": "number", "id": "add_secs", "min": 1, "max": 3600, "default": 1 },
    { "type": "button", "command": "add", "args_from": { "seconds": "add_secs" } },
    { "type": "separator" }
  ]
}
```

- **`display_name`:** required, 1–128 bytes.
- **`commands`:** optional, at most 64, with unique IDs.
  - `label`: ≤ 64 bytes, defaults to the ID.
  - `description`: ≤ 256 bytes.
  - `confirm`: boolean.
  - `args`: at most 16, each mapping a name to `"int"`, `"number"`, `"string"` or `"bool"`.
  - Any invalid command rejects the whole registration.
- **`dock`:** optional, at most 256 controls in total, counting row items. The control types are:
  - `label`: needs `bind` (a state key); `style` is optional.
  - `button`: needs `command` (a declared command); `args_from` is optional and maps declared args to `number`/`text` control IDs.
  - `toggle`: needs `bind` and `command`; it sends `{"value": true|false}`.
  - `number`: needs a unique `id`; `min`, `max` and `default` are optional, with min ≤ default ≤ max.
  - `text`: needs a unique `id`; `default` is optional, ≤ 256 bytes.
  - `row`: holds `items`; rows can't be nested.
  - `separator`.

  Unknown control types, and controls with invalid fields, are **skipped with a logged warning**, and the registration still succeeds. That way newer scripts degrade gracefully on older plugins. Unknown fields are ignored.

## State

`luabridge_set_state` takes a JSON object of key → value:
- Values must be a string, number or boolean.
- `null` deletes the key.
- Arrays and objects are rejected.
- An owner can have at most 256 keys.

An update is all-or-nothing: if any key is invalid or the limit would be exceeded, nothing changes. Registering again clears the owner's state.

## Heartbeat

`luabridge_heartbeat` is optional. Once an owner has sent a heartbeat, it becomes **stale** if no further heartbeat arrives within 30 s. While stale:
- `luabridge_run_command` calls to that owner fail with `owner is stale; register again`, and so do its own heartbeats.
- `set_state` and `emit` keep working.

Registering again clears the stale status. Owners that never send a heartbeat never go stale.

## Limits and IDs

| Item | Rule |
|---|---|
| Owner ID | `[a-z0-9_.-]{1,64}`, e.g. `stopwatch`, `grumpydog.scoreboard`; duplicated scripts add a suffix |
| Command, arg and control IDs | `[a-z0-9_]{1,64}` |
| State keys and event names | `[a-zA-Z0-9_.]{1,64}` |
| Any JSON argument | ≤ 65,536 bytes, valid UTF-8; must be an object |
| Owners | ≤ 128 |
| Commands per owner | ≤ 64 |
| State keys per owner | ≤ 256 |

For `emit` and `run_command`, an empty `json` counts as `{}`. `run_command` checks the JSON against the command's declared args:
- undeclared keys are rejected;
- `int` must be a whole number;
- missing args are allowed.

## Error messages

These are stable, so scripts may match on them:

| Error | Cause |
|---|---|
| `missing owner` / `missing json` | Argument not set or empty |
| `invalid owner id '<id>'` | Owner ID doesn't match the rule above |
| `invalid command id '<id>'` / `invalid event name '<name>'` / `invalid state key '<key>'` | ID doesn't match its rule |
| `json exceeds 65536 bytes` | JSON argument too large |
| `invalid JSON` | Not parseable, or invalid UTF-8 |
| `json must be an object` | Top-level JSON is not an object |
| `display_name must be a string of 1-128 bytes` | Bad or missing display name |
| `commands must be an array` / `commands[<i>]: <reason>` / `duplicate command '<id>'` | Bad command list |
| `too many owners (max 128)` / `too many commands (max 64)` / `too many args (max 16)` / `too many dock controls (max 256)` / `too many state keys (max 256)` | Limit exceeded |
| `dock must be an array` | Bad dock |
| `state value for '<key>' must be a string, number, boolean or null` | Bad state value |
| `owner not registered` | The owner has no registration |
| `owner is stale; register again` | Heartbeat timed out (see Heartbeat) |
| `unknown command '<id>'` / `unknown argument '<name>'` / `argument '<name>' must be <type>` | `run_command` didn't match the declaration |
| `plugin unloaded` | Called during OBS shutdown, after the plugin unloaded (never returned by `luabridge_unregister`) |
