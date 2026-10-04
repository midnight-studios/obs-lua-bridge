# StopWatch integration

This page defines how the StopWatch script ([midnight-studios/obs-lua](https://github.com/midnight-studios/obs-lua)) plugs into Lua Bridge for OBS. `lua/examples/stopwatch-demo.lua` in this repository is a **demo** that implements the same contract. Dock layouts, Stream Deck buttons and other websocket clients built against the demo therefore keep working when the real script adopts the integration.

> **Don't run the demo and the real StopWatch at the same time.** Both use the owner `stopwatch`. The second one to load replaces the first one's registration, and the plugin logs `owner 'stopwatch' was already registered and active; its registration was replaced`. Load only one of them.

## Contract

Clients address the stopwatch by **owner**, never by script filename, so renaming or moving the script doesn't break anything.

**Owner:** `stopwatch`. Further instances of a duplicated script use a suffix, e.g. `stopwatch.2`.

**Commands:**

| Command | Args | Effect |
|---|---|---|
| `toggle` | — | Start if paused or stopped, pause if running (the dock's Start/Pause button) |
| `start` | — | Start (no change if running) |
| `pause` | — | Pause (no change if paused) |
| `reset` | — | Stop and set the time to 0 (`confirm: true` in the dock) |
| `add` | `seconds: int` | Add time |
| `subtract` | `seconds: int` | Subtract time; never below 0 |

**State:**

| Key | Type | Meaning |
|---|---|---|
| `display` | string | The formatted time shown in the dock and text source, e.g. `00:01:15` |
| `elapsed` | int | Elapsed whole seconds |
| `running` | bool | `true` while counting |
| `toggle_label` | string or unset | `"Pause"` while running, `"Resume"` when paused with time on the clock. Unset after reset, so the button shows its own label, `"Start"`. Used by the Start/Pause button's `label_bind`. |

**Dock:**
- a large `display` label;
- a row with **Start/Pause** (`toggle`, `label_bind: "toggle_label"`) and **Reset**;
- a "Seconds" number input;
- a row with **− seconds** / **+ seconds** (`subtract` / `add`, `args_from: {seconds: "step"}`).

The script may add more state keys or commands later; additions don't break clients.

## Drop-in snippet

Add `luabridge.lua` next to the StopWatch script, then add the following, mapping the commands to the script's own functions. The names below (`start_timer`, `pause_timer`, `reset_timer`, `adjust_timer`, `is_running`, `elapsed_seconds`, `format_time`) are placeholders.

```lua
local bridge = dofile(script_path() .. "luabridge.lua")

local REGISTRATION = { --[[ copy the registration table from lua/examples/stopwatch-demo.lua ]] }

local function publish() -- call whenever the time or running state changes
	local seconds = elapsed_seconds()
	local label = bridge.null                     -- "Start"
	if is_running() then label = "Pause" elseif seconds > 0 then label = "Resume" end
	bridge.set_state("stopwatch", {
		display = format_time(seconds), elapsed = seconds, running = is_running(), toggle_label = label,
	})
end

local commands = {
	toggle = function() if is_running() then pause_timer() else start_timer() end end,
	start = start_timer,
	pause = pause_timer,
	reset = reset_timer,
	add = function(args) adjust_timer(args.seconds or 0) end,
	subtract = function(args) adjust_timer(-(args.seconds or 0)) end,
}

-- in script_load (all of this does nothing when the plugin isn't installed):
bridge.register("stopwatch", REGISTRATION)
bridge.on_command("stopwatch", function(command, args)
	if commands[command] then commands[command](args); publish() end
end)
publish()

-- in script_unload:
bridge.shutdown()
```

The helper sends heartbeats and registers the owner again if the plugin forgets it, for example after **Remove** in the dock, so the script needs no extra code for that. Without the plugin installed, every call returns `false` and the StopWatch works exactly as before.

## Checklist for the real script
- ☐ All commands and state keys above are implemented.
- ☐ `publish()` is called on every start, pause, reset and adjust, and at least once per second while running (only when the displayed second changes).
- ☐ The script works unchanged with the plugin removed.
- ☐ `tests/websocket/test_examples.py` (`test_stopwatch_by_owner`) passes with the real script loaded instead of the demo.
