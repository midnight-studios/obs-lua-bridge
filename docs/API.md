# Lua Bridge for OBS: script API (api_version 1)

This is the contract between the plugin and scripts. The plugin exposes **procedures** that scripts call, and emits **signals** that scripts connect to. All structured data is passed as UTF-8 JSON strings.

Scripts must keep working without the plugin. Call `luabridge_get_info` first. If the call fails (the procedure doesn't exist), the plugin isn't installed, so skip everything else, including connecting to the `luabridge_*` signals.

## The helper library: `luabridge.lua`

Most scripts should use the helper rather than calling procedures directly. Copy `lua/luabridge.lua` next to your script and load it:

```lua
local bridge = dofile(script_path() .. "luabridge.lua")

function script_load(settings)
	bridge.register("myscript", {
		display_name = "My Script",
		commands = { { id = "go", label = "Go" } },
		dock = { { type = "label", bind = "status" }, { type = "button", command = "go" } },
	})
	bridge.on_command("myscript", function(command, args, origin)
		bridge.set_state("myscript", { status = "went (" .. origin .. ")" })
	end)
end

function script_unload()
	bridge.shutdown()
end
```

Every function returns `ok, err` and never throws. **Without the plugin**, every call returns `false, "Lua Bridge plugin not available"` and does nothing, so the same script runs unchanged where the plugin isn't installed.

| Function | Purpose |
|---|---|
| `bridge.available()` | `true` if the plugin is installed and compatible, otherwise `false, reason` |
| `bridge.unavailable_reason()` | `nil`, or why the plugin can't be used |
| `bridge.info()` / `bridge.has(capability)` | The decoded `luabridge_get_info` table; whether a capability flag is `true` |
| `bridge.register(owner, registration, options)` | Registration as a Lua table. `options`: `heartbeat` (default `true`), `heartbeat_interval` (seconds, default 10), `on_heartbeat(ok, err)`. Registering clears the owner's state. |
| `bridge.unregister(owner)` | Removes an owner |
| `bridge.on_command(owner, fn)` | `fn(command, args, origin)` for each command; `args` is a table |
| `bridge.on_event(fn)` | `fn(owner, event, data)` for every `luabridge_emit` |
| `bridge.set_state(owner, values)` | Merges values; `bridge.null` deletes a key |
| `bridge.emit(owner, event, data)` | Custom event to scripts and websocket clients |
| `bridge.run_command(owner, command, args)` | Sends a command to any owner (`origin = "script"`) |
| `bridge.set_heartbeat(owner, enabled)` | Turns the heartbeat on or off; turning it on sends one at once |
| `bridge.shutdown()` | Unregisters every owner of the script, stops the heartbeat, disconnects signals. Call it from `script_unload`. |
| `bridge.json.encode(value)` / `bridge.json.decode(text)` | Pure-Lua JSON. Returns the result, or `nil, error`. |
| `bridge.null`, `bridge.array(t)`, `bridge.object(t)` | JSON `null`, and markers for an empty array or object (an empty table encodes as `{}`) |
| `bridge.VERSION`, `bridge.API_VERSION` | Helper version (`"1.0.0"`), and the plugin `api_version` it needs (1) |

**Automatic behaviour:**
- **Heartbeat:** one shared timer sends a heartbeat for each owner (every 10 s by default), so an owner only goes stale if the script really stops.
- **Re-registration:** if the plugin answers `owner not registered` or `owner is stale; register again`, for example after **Remove** in the dock, the helper registers the owner again with its original registration, republishes the state it has set, and retries the call once. It logs `[luabridge] re-registered '<owner>'`, at most once per owner every 5 s.
- **Compatibility:** if the plugin's `api_version` is missing or older than `bridge.API_VERSION`, the helper treats the plugin as unavailable and logs one warning explaining that the plugin needs updating. Newer plugins work, because API changes within a major version are additive.
- **Signals:** command and event handlers are connected once per script and disconnected by `shutdown()`. Errors in your handlers are caught and logged, never passed back to OBS.

**JSON details:**
- Object keys are sorted, so output is deterministic.
- Integers are written without a decimal point; other numbers in the shortest form that reads back exactly.
- A table with keys `1..n` is an array.
- Decoded objects and arrays keep their shape when encoded again, and JSON `null` decodes to `bridge.null`.
- Decoding errors report the position, e.g. `expected ':' at position 5`.

Examples: `lua/examples/hello-bridge.lua` (smallest), `stopwatch-demo.lua`, `scoreboard.lua`, and `tests/lua/dock-test.lua` (every dock control).

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
| `luabridge_unregister` | `string owner` | — | Remove everything for this owner. Call it from `script_unload`. Always succeeds for a valid owner ID; for an owner that isn't registered it is a silent no-op. |
| `luabridge_set_state` | `string owner`, `string json` | — | Merge key/values into the owner's state. |
| `luabridge_emit` | `string owner`, `string event`, `string json` | — | Send a custom event to other scripts (`luabridge_event`) and to websocket clients (`CustomEvent`). The data can't contain `null` or arrays of non-objects (see [Event data](#event-data)). |
| `luabridge_heartbeat` | `string owner` | — | Optional liveness ping (see Heartbeat). |
| `luabridge_run_command` | `string owner`, `string command`, `string json` | — | Invoke another owner's command. The target receives `luabridge_command` with `origin = "script"`. |

`luabridge_run_command` is an addition to spec B5, which has no way for a script to invoke a command. It's what lets B6's `origin = "script"` happen.

### `luabridge_get_info` JSON

```json
{"api_version":1,"plugin_version":"0.1.0","obs_version":"32.2.2","capabilities":{"commands":true,"events":true,"heartbeat":true,"run_command":true,"state":true,"websocket":true}}
```

- `capabilities` is an object of flags. It isn't an array because the same JSON is sent over obs-websocket, which can't carry arrays of strings.
- `websocket` is `true` only when obs-websocket is available and the `LuaBridge` vendor registered.
- Check a flag before relying on a feature, and treat a missing flag as `false`, since future versions only add flags. Without a JSON decoder, a string search is enough: `info:find('"websocket":true', 1, true) ~= nil`. The M4 helper library adds a decoder.

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
  - `button`: needs `command` (a declared command); `args_from` is optional and maps declared args to `number`/`text` control IDs. `label_bind` is optional: a state key whose value the button shows while it's set (see [The dock](#the-dock)).
  - `toggle`: needs `bind` and `command`; it sends `{"value": true|false}`, so the command must declare an arg `value` of type `bool`.
  - `number`: needs a unique `id`; `min`, `max` and `default` are optional, with min ≤ default ≤ max. `label` (text shown next to it) is optional.
  - `text`: needs a unique `id`; `default` is optional, ≤ 256 bytes. `label` is optional.
  - `row`: holds `items`; rows can't be nested.
  - `separator`.

  Unknown control types, and controls with invalid fields, are **skipped with a logged warning**, and the registration still succeeds. That way newer scripts degrade gracefully on older plugins. Unknown fields are ignored.

**Registering an owner that is already active** (registered and not stale) replaces it and logs a warning: `owner '<id>' was already registered and active; its registration was replaced (are two scripts using the same owner?)`. The call still succeeds. A script that reloads doesn't trigger it, because it unregisters first; neither does re-registering a stale owner. It usually means two scripts use the same owner ID.

## State

`luabridge_set_state` takes a JSON object of key → value:
- Values must be a string, number or boolean.
- `null` deletes the key.
- Arrays and objects are rejected.
- An owner can have at most 256 keys.

An update is all-or-nothing: if any key is invalid or the limit would be exceeded, nothing changes. Registering again clears the owner's state.

## Event data

The JSON passed to `luabridge_emit` is also sent to websocket clients. obs-websocket uses `obs_data`, which silently drops `null` values and array elements that aren't objects. Rather than lose data without notice, `luabridge_emit` rejects such data with `json cannot contain null or arrays of non-objects`:
- **Allowed:** objects, strings, numbers, booleans, empty arrays, and arrays of objects (at any depth).
- **Rejected:** `{"a":null}`, `{"tags":["a","b"]}`, `{"n":[1,2]}`, and nested arrays.
- **Workaround for a list of scalars:** wrap each element, e.g. `{"tags":[{"v":"a"},{"v":"b"}]}`, or use an object keyed by the values.

## Heartbeat

`luabridge_heartbeat` is optional. Once an owner has sent a heartbeat, it becomes **stale** if no further heartbeat arrives within 30 s. While stale:
- `luabridge_run_command` calls to that owner fail with `owner is stale; register again`, and so do its own heartbeats.
- `set_state` and `emit` keep working.

Registering again clears the stale status. Owners that never send a heartbeat never go stale.

## Logging

Lua Bridge keeps the OBS log quiet in normal use:
- **Once per start:** `plugin loaded`, whether the obs-websocket vendor registered, and `plugin unloaded`.
- **Registering and unregistering owners** is logged at debug level, which OBS only writes with `--verbose`.
- **Failed calls** log a warning: `<procedure>(<owner>): <error>` for scripts, `websocket <request>(<owner>): <error>` for websocket clients. The same failure (same procedure, owner and error) repeated within 10 s is logged once. The next one after that ends with `(N similar lines suppressed in the last 10 s)`. A script that calls something wrong in a loop therefore can't flood the log.
- **Registration warnings** (a dock control skipped, an owner replaced while active) are logged for every registration.
- **The helper library** deduplicates its own handler-failure warnings the same way, per handler.

## Duplicated scripts and instance IDs

OBS can't load the same script file twice, so running two copies of a script means two files. Each copy needs its own **owner**: use the base owner plus a suffix, e.g. `stopwatch` and `stopwatch.2`. The examples `stopwatch-demo.lua` and `scoreboard.lua` have an **Instance ID** script property for this. When it's empty they use the base owner; `2` gives `stopwatch.2`. Changing it registers the script under the new owner. Two scripts using the same owner replace each other's registration, which the plugin logs as a warning.

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
- missing args are allowed;
- a `null` value counts as a missing arg. It's removed before the command is delivered, so scripts never receive `null` args. This applies to commands from scripts, the dock and websocket alike.

## The dock

The plugin adds one dock, **Lua Bridge** (Docks → Lua Bridge). It has one collapsible section per registered owner, titled with its `display_name`, in registration order. OBS remembers the dock's position, size and visibility. When no owner is registered, the dock shows a short placeholder text.

**How the controls appear:**

| Control | Shown as | What it does |
|---|---|---|
| `label` | Text bound to a state key | Shows the key's value (strings as-is, numbers in shortest form, `true`/`false`, `—` when not set) and updates as soon as the script calls `luabridge_set_state`. `style: "large"` shows it large and bold. |
| `button` | A button with the command's `label` (tooltip: `description`) | Sends the command with `origin = "dock"`. With `confirm: true` it asks first. `args_from` fills args from `number`/`text` controls. With `label_bind: "<state key>"`, the button shows that key's value while it's set, and the command's label when it's unset, `null` or empty. Use it for e.g. Start/Pause, or "Starting…" → "Started ✓". |
| `row` | Its items side by side | Buttons share the width. |
| `number` | A spin box (whole numbers, or 3 decimals if `min`, `max` or `default` isn't whole) | Holds a value for buttons; sends nothing by itself. |
| `text` | A text field | Holds a value for buttons; sends nothing by itself. |
| `toggle` | A checkbox with the command's `label`, checked when the bound key is `true` | A click sends `{"value": <new>}`. The box then shows the **state again** until the script sets it, so it always shows the script's view. |
| `separator` | A horizontal line | — |

**Commands from the dock:**
- They're validated like any other command. A failure (for example `owner is stale; register again`) appears under the section for a few seconds and is logged.
- They reach the script asynchronously on the UI thread, like commands from scripts and websocket.

**Reloading a script** replaces its section in place, with no duplicates, keeping its position and whether it was collapsed.

**Stale owners:**
- When an owner that sends heartbeats goes stale (see [Heartbeat](#heartbeat)), its section stays but its controls are disabled and the title shows "— not responding".
- A **Remove** button appears in the section's title. It unregisters the owner, exactly like `luabridge_unregister`: the section disappears, and websocket clients no longer list the owner. This clears sections left behind by scripts that crashed or were removed without unregistering.
- If the script is still running, its `set_state`/`emit` calls now fail with `owner not registered`. That's the cue to call `luabridge_register` again, after which the section reappears.
- Only stale sections can be removed from the dock.

## obs-websocket vendor API

The plugin registers the obs-websocket vendor **`LuaBridge`**. Clients call its requests with obs-websocket's `CallVendorRequest` (`vendorName: "LuaBridge"`), and receive its events as `VendorEvent`, which needs the `Vendors` event subscription.

Every response is an object with `"ok": true|false`. On failure it also has `"error"`, using the same strings as the script API (see [Error messages](#error-messages)). This is necessary because obs-websocket always reports vendor requests themselves as successful. A request type that doesn't exist *is* rejected by obs-websocket itself.

### Requests

| Request | Request data | Response on success |
|---|---|---|
| `GetInfo` | — | `{ok, api_version, plugin_version, obs_version, capabilities}`. Same fields as `luabridge_get_info`. |
| `ListOwners` | — | `{ok, owners: [{owner, display_name, stale}]}`, sorted by owner |
| `ListCommands` | `{owner}` | `{ok, owner, commands: [{id, label, description, confirm, args: {name: type}}]}`, in declaration order |
| `GetState` | `{owner}` | `{ok, owner, state: {key: value}}` |
| `RunCommand` | `{owner, command, data?}` | `{ok, accepted: true}` |

- **Fire and forget:** `RunCommand` is validated exactly like `luabridge_run_command` (owner, command, size, declared args, stale owner). The script receives `luabridge_command` with `origin = "websocket"` asynchronously, after the response has been sent. Scripts report results through state or events.
- **Field types:** `owner` and `command` must be strings, and `data` (optional) must be an object. Otherwise the error is `missing owner`, `owner must be a string`, `missing command`, `command must be a string` or `data must be an object`.
- **What reaches the script:** every value valid for a declared arg (int, number, string, bool) arrives unchanged.
  - A `null` argument counts as omitted and is removed before the script sees it. That makes behaviour the same on every obs-websocket version: 5.6 drops `null` before the plugin sees it, while 5.7 passes it through. A `null` for an undeclared name is ignored too.
  - An array keeps its key but loses its contents, so it then fails validation (`argument '<name>' must be <type>` or `unknown argument '<name>'`).
- **Threading:** requests are handled on obs-websocket's own threads, possibly several at once. The registry is thread-safe, and signals to scripts are still delivered on the UI thread, in order.
- **During shutdown:** requests answer `{"ok":false,"error":"plugin unloaded"}`.

### Rate limiting

`RunCommand` is rate limited, because every accepted command becomes work on OBS's UI thread. obs-websocket doesn't tell the plugin which client sent a request, so the limits are per **target owner**, plus one global limit:

| Limit | Sustained | Burst |
|---|---|---|
| Per owner | 30 commands/s | 60 |
| All owners together | 200 commands/s | 400 |

That's far above what people or controllers like a Stream Deck send.

**When a request is over the limit:**
- The client gets `{"ok": false, "error": "rate limited; retry in 120 ms", "retry_after_ms": 120}`. `retry_after_ms` appears only on this error, and is the time until one more command for that owner fits.
- The command is **not** queued or delivered.
- Other owners are unaffected unless the global limit is reached.

**How it's logged** (without flooding the log):
- The first rejection logs one warning: `websocket RunCommand(<owner>): rate limited (30/s, burst 60)`.
- While requests keep being rejected, at most one line per owner every 10 s, ending with `; N more requests were rate limited in the last 10 s`.

Other requests (`GetInfo`, `ListOwners`, `ListCommands`, `GetState`) are read-only and aren't rate limited. Commands from scripts (`luabridge_run_command`) and from the dock aren't rate limited either, because they come from inside OBS.

### Events

| Event | Data | Sent when |
|---|---|---|
| `StateChanged` | `{owner, changes: {key: value}, removed?: {key: true}}` | `luabridge_set_state` changed at least one key. `changes` holds new and updated values. `removed` lists deleted keys and is present only when keys were deleted. |
| `CustomEvent` | `{owner, event, data}` | `luabridge_emit` succeeded. `data` is the emitted object. |

Deleted keys can't be reported as `"key": null` because obs-websocket drops `null`, hence `removed`. Events from one script arrive in the order its calls were made. No events are sent once OBS starts shutting down.

### Without obs-websocket

If obs-websocket isn't installed or failed to load:
- the plugin logs `[lua-bridge] obs-websocket not available; websocket requests and events disabled` once;
- `capabilities.websocket` is `false`;
- everything else works unchanged.

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
| `json cannot contain null or arrays of non-objects` | `luabridge_emit` data that obs-websocket couldn't carry (see [Event data](#event-data)) |
| `owner must be a string` / `command must be a string` / `missing command` / `data must be an object` | Websocket request fields of the wrong type |
| `rate limited; retry in <N> ms` | Websocket `RunCommand` over the rate limit (see [Rate limiting](#rate-limiting)); the response also has `retry_after_ms` |
| `plugin unloaded` | Called during OBS shutdown, after the plugin unloaded (never returned by `luabridge_unregister`) |
