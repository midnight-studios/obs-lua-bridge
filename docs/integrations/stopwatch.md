# StopWatch integration

The StopWatch script ([midnight-studios/obs-lua](https://github.com/midnight-studios/obs-lua)) works with Lua Bridge from version 6.2. **How to set it up, its settings, and everything it adds** are in StopWatch's own guide: [StopWatch and the Lua Bridge](https://github.com/midnight-studios/obs-lua/blob/main/docs/STOPWATCH-LUABRIDGE.md).

- **The dock:** StopWatch appears as a section in **Docks → Lua Bridge**. For a dock of its own, open **Tools → Lua Bridge** and tick **Dock** next to it (see "A dock per script" in [API.md](../API.md#a-dock-per-script)).
- **Remote control:** obs-websocket clients address it by its owner, `stopwatch`, as below.

`lua/examples/stopwatch-demo.lua` in this repository is a **demo** of the same contract, for trying Lua Bridge without StopWatch and as a reference for other scripts. Dock layouts, Stream Deck buttons and other websocket clients built against the demo keep working with the real script.

> **Don't run the demo and the real StopWatch at the same time.** Both use the owner `stopwatch`. The second one to load replaces the first one's registration, and the plugin logs `owner 'stopwatch' was already registered and active; its registration was replaced`. Load only one of them.

## Contract

Clients address the stopwatch by **owner**, never by script filename, so renaming or moving the script doesn't break anything. The demo and StopWatch 6.2 both implement this; StopWatch adds more commands, state keys and events, listed in its guide. Additions don't break clients.

**Owner:** `stopwatch`. Further copies of the script use a suffix, e.g. `stopwatch.2`.

**Commands:**

| Command | Args | Effect |
|---|---|---|
| `toggle` | — | Start if paused or stopped, pause if running (the dock's Start/Pause button) |
| `start` | — | Start (no change if running) |
| `pause` | — | Pause (no change if paused) |
| `reset` | — | Stop and return to the start time: 0 when counting up, the set time when counting down (`confirm: true` in the dock) |
| `add` | `seconds: int` | Add time |
| `subtract` | `seconds: int` | Subtract time; never below 0 (StopWatch: unless its user turned "Prevent Negative Time Value" off) |

`add` and `subtract` without `seconds` do nothing. (The plugin delivers a command whose declared args are missing, and removes `null` args.)

**State:**

| Key | Type | Meaning |
|---|---|---|
| `display` | string | The time as `HH:MM:SS`, e.g. `00:01:15`, whatever time format the script shows on screen. Hours go past 24. StopWatch adds `+` in overtime and `-` for negative time. |
| `elapsed` | int | The whole seconds on the clock: counting down, the time remaining. Negative only when StopWatch's user allows negative time. (The name is kept for compatibility.) |
| `running` | bool | `true` while counting |
| `toggle_label` | string or unset | `"Pause"` while running, `"Resume"` when paused with time on the clock. Unset after reset, so the button shows its own label, `"Start"`. Used by the Start/Pause button's `label_bind`. |

**Dock** (the demo's layout; StopWatch shows more, see its guide):
- a large `display` label;
- a row with **Start/Pause** (`toggle`, `label_bind: "toggle_label"`) and **Reset**;
- a "Seconds" number input;
- a row with **− seconds** / **+ seconds** (`subtract` / `add`, `args_from: {seconds: "step"}`).

## Testing a script against the contract

`py tests/websocket/test_examples.py` runs the Stopwatch tests (`test_stopwatch_by_owner`, `test_stopwatch_validates_args`) whenever the owner `stopwatch` is registered, with either the demo or StopWatch loaded; the Score Board tests are skipped when `scoreboard.lua` isn't loaded. StopWatch 6.2 passes them (Count Up, with Lua Bridge 0.9.0-beta2, on Windows). Count Down and StopWatch's own time formats are tested in StopWatch's repository.
