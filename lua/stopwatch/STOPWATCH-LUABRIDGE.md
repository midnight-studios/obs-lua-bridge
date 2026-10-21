# StopWatch and the Lua Bridge

StopWatch is MIT-licensed. Copyright (c) Midnight Studios. Licence text: [lua/LICENSE](https://github.com/midnight-studios/obs-lua-bridge/blob/master/lua/LICENSE) in the Lua Bridge repository.

StopWatch 6.2 works with the optional [Lua Bridge](https://github.com/midnight-studios/obs-lua-bridge) plugin. With the plugin installed you get:

- **a StopWatch section in Docks → Lua Bridge:** the time, the status, and buttons for the timer;
- **remote control:** obs-websocket clients (Bitfocus Companion, Stream Deck plugins, your own tools) can run the timer's commands, read its state and receive its events.

Without the plugin, StopWatch works as 6.1, apart from the 6.2 bug fixes (see the script header). Nothing needs to be turned off.

This page is for StopWatch users who want the dock or remote control, and for anyone writing an integration.

## Setting it up

1. Install the Lua Bridge plugin (version 0.9.0-beta1 or later; it needs OBS Studio 31.1 or newer) and restart OBS.
2. Load StopWatch 6.2 in Tools → Scripts, as usual.
3. Open Docks → Lua Bridge. The StopWatch section appears there once OBS has finished loading.

**The helper file.** StopWatch talks to the plugin through the helper `luabridge.lua`. It looks for it in two places, in this order:

1. next to the StopWatch script, if you put a copy there;
2. the copy installed with the plugin.

You normally don't need to do anything: the plugin installs its helper. If the script reports that the helper is missing, put `luabridge.lua` next to the script, or reinstall the plugin. Finding the plugin's copy is tested on Windows only; on macOS and Linux, a copy next to the script always works.

**Settings.** Both settings are in the script's properties:

- **Disable Features → "Lua Bridge (dock and remote control)"** (on by default). Turn it off to remove the section from the dock. The timer keeps running.
- **The "Lua Bridge" group** (Advanced layout, with Timer Settings expanded) shows:
  - **Status:** whether the plugin is connected, and what to do if it isn't;
  - **Instance ID:** see [Two copies of the script](#two-copies-of-the-script);
  - a short help text with a link to the plugin.

  The group is hidden in the Basic layout. The bridge keeps working there.

The status is also written to the script log when the script loads, and again whenever it changes. These are the messages:

| Status | Meaning |
|---|---|
| `Lua Bridge: connected (plugin <version>, owner '<owner>', helper: <file or plugin>).` | Working |
| `Lua Bridge: not installed (optional). StopWatch works the same without it.` | No plugin |
| `Lua Bridge: plugin found, but its helper luabridge.lua wasn't found. Put luabridge.lua next to this script, or reinstall the Lua Bridge plugin.` | No helper |
| `Lua Bridge: the plugin is too old for this script. Update the Lua Bridge plugin. (...)` | Update the plugin |
| `Lua Bridge: the plugin reported an error, so the dock section is off. Restart OBS, or reinstall the Lua Bridge plugin. (...)` | Plugin error |
| `Lua Bridge: turned off in this script's settings (Disable Features).` | The feature is off |
| `Lua Bridge: the plugin refused this script's registration (...).` | A StopWatch bug: logged as a warning (please report it) |
| `Lua Bridge: waiting for OBS to finish loading.` | Panel only, while OBS is still loading |

## The dock section

The section is called **StopWatch** (or **StopWatch <id>** for a second copy). From top to bottom it shows:

| Control | Shown |
|---|---|
| The time, in your Time Format | Always |
| The status, e.g. `Countdown · Running` | Always |
| **Start / Pause / Resume** and **Reset** (Reset asks for confirmation) | Always |
| **Split** | When "Timer Time Split/Lap" is on (Disable Features) and a Split Source is set |
| **Set** | In Count Up |
| **Direction** | When "Enable Timer Direction Toggle" is on |
| A **Seconds** box (1–3600, default 10), with **−** and **+** buttons | Always (they do nothing while Timer Manipulation is on and set to Disabled) |
| The **Add** and **Subtract** preset buttons, captioned with their steps, e.g. `+30 s (2 left)` | When Timer Manipulation is on and not set to Disabled |

The section rebuilds itself when one of these settings changes. Other settings changes (a prefix, a colour) don't touch it.

## Remote control (obs-websocket)

The plugin adds the vendor **`LuaBridge`** to obs-websocket. StopWatch's **owner** is `stopwatch`.

### Running a command

Send the vendor request **`RunCommand`** with the owner, the command, and its arguments (if any) under `data`:

```json
{
  "vendorName": "LuaBridge",
  "requestType": "RunCommand",
  "requestData": { "owner": "stopwatch", "command": "add", "data": { "seconds": 30 } }
}
```

The request is fire-and-forget: the reply only says whether the plugin accepted it (`ok`, or an error), and the result shows up in the state and the events. The plugin rejects a request for an unknown owner, a command the script didn't declare, an argument of the wrong type (`seconds` must be a whole number), and requests above its rate limit (30 a second per owner, bursts of 60).

**Companion example.** In Bitfocus Companion, with the OBS connection set up, add the action **"Send Vendor Request"** (obs-websocket module) to a button:

| Field | Value |
|---|---|
| vendorName | `LuaBridge` |
| requestType | `RunCommand` |
| requestData | `{"owner":"stopwatch","command":"toggle"}` |

That button now starts and pauses the timer. For a button caption that follows the timer (Start, Pause, Resume), use the state key `toggle_label`.

### Commands

| Command | Arguments | What it does |
|---|---|---|
| `toggle` | — | Starts or pauses the timer. It starts even when Start and Pause are split into separate buttons |
| `start` | — | Starts the timer; no change if it is running |
| `pause` | — | Pauses the timer; no change if it is paused |
| `reset` | — | A user Reset, as with the Reset button: the timer stops and goes back to its start time (0 in Count Up, the set time in Count Down; for a countdown to a date, the time now remaining to the date). It also clears the Autoload time |
| `add` | `seconds` (integer) | Adds the seconds to the clock |
| `subtract` | `seconds` (integer) | Subtracts the seconds. The clock doesn't go below 0, unless the direction was changed with "Prevent Negative Time Value" off |
| `set_time` | `seconds` (integer) | Sets the clock to the seconds. The start time that Reset uses doesn't change |
| `split` | — | Records a split time; only while the timer runs, as the hotkey does |
| `set` | — | Count Up only: sets the timer to the time in "Manually set timer", as the Set button does |
| `toggle_mili` | — | Shows or hides the hundredths; only when "Split Seconds Toggle" is on |
| `toggle_direction` | — | Counts the other way; only when "Enable Timer Direction Toggle" is on |
| `add_preset_1` … `add_preset_3` | — | Adds Add Seconds Set 1–3, with its limit, as the hotkey does; only when Timer Manipulation is on and not set to Disabled |
| `subtract_preset_1` … `subtract_preset_3` | — | Subtracts Subtract Seconds Set 1–3, under the same rules |

**Rules that apply to every command:**

- **`add`, `subtract` and `set_time`** do nothing when `seconds` is missing, not a number, negative, or over 100 days (8,640,000).
- **Timer Manipulation set to Disabled** blocks `add`, `subtract`, `set_time` and the presets. When the Timer Manipulation feature is off, `add`, `subtract` and `set_time` still work, but the presets don't.
- **Count Down to a specific date and time:** an `add`, `subtract` or `set_time` lasts until the next Start, which shows the real remaining time again (as in 6.1).
- **Disable Script:** every command is ignored, and the status shows `disabled`.
- **No Timer Source selected:** commands still change the time, but nothing is shown until a source is selected (as in 6.1). The status is `no_source`.

### State

Read the whole state with the vendor request **`GetState`** and `{"owner": "stopwatch"}`. The reply holds the keys under `state`. Changes arrive as the vendor event **`StateChanged`**, `{owner, changes}`, holding only the keys that changed.

The time keys (`display`, `elapsed`, `time`, `text`) are sent at most every 200 ms while the timer runs, and at once whenever the whole second changes. All other keys are sent as soon as they change.

A key that has no value (for example `last_split` before the first split) is unset: it is missing from `GetState`, and a change to unset arrives as `null`.

| Key | Type | Value |
|---|---|---|
| `display` | string | The clock as `HH:MM:SS`, whatever your Time Format. Hours can go past 24 (`49:00:00`). Overtime starts with `+`, negative time with `-` |
| `elapsed` | integer | The whole seconds on the clock. In Count Down this is the time remaining (the name comes from the Lua Bridge's standard StopWatch contract) |
| `running` | boolean | The timer is running |
| `toggle_label` | string or unset | `Pause` while running, `Resume` when stopped away from the start time (also after a Count Down has expired at 0), otherwise unset (the dock button then reads Start) |
| `time` | string | The time in your Time Format, without prefix or suffix. In overtime it has the `+` when "Overtime: show '+' and Marker B colour" is on (the default) |
| `text` | string | Exactly what the timer text source shows: prefix, time and suffix, or the Timer Expired Text after a TIMER END TEXT expiry |
| `mode` | string | `stopwatch` (Count Up) or `countdown` |
| `direction` | string | `up` or `down`: the direction the clock counts now |
| `status` | string | One of `disabled`, `no_source`, `overtime`, `running`, `expired`, `paused`, `stopped` (see below) |
| `status_text` | string | The status in words, as the dock shows it: `Countdown · Running`, `Count Up · Paused`, `Select a Timer Source in the script settings` |
| `expired` | boolean | `true` when a Count Down has run out and stopped at 0, until the next Reset or Start, or until the clock is moved off 0 |
| `overtime` | boolean | The timer is counting overtime. It becomes `false` on Reset, and when the direction was toggled in overtime and the clock counts back through 0: the timer then counts Count Down time again (no event) |
| `split_count` | integer | The number of splits since the last Reset |
| `last_split` | string or unset | The time of the last split (its mark), in the split text's format. With Split Type "Interval" the split text shows the interval instead; the `split` event has both |
| `next_marker_a`, `next_marker_b` | string or unset | The Marker A / Marker B time that fires next, e.g. `00:00:30`. Unset when there is none |
| `add_1_label` … `add_3_label` | string | The Add preset captions, e.g. `+30 s`, `+1 min`, `+30 s (2 left)` with a limit |
| `sub_1_label` … `sub_3_label` | string | The Subtract preset captions, e.g. `−1 min` (with a real minus sign, U+2212) |
| `version` | string | The script version, `6.2` |
| `disabled` | boolean | Disable Script is ticked |

**The status** is the first of these that applies:

1. `disabled`: Disable Script is ticked;
2. `no_source`: no Timer Source is selected;
3. `overtime`: counting overtime;
4. `running`;
5. `expired`: a Count Down that has run out, stopped at 0. A Reset never gives `expired`, not even with a start time of 0 or a date that has passed;
6. `paused`: stopped away from the start time;
7. `stopped`: at the start time.

### Events

Events arrive as the vendor event **`CustomEvent`**, `{owner, event, data}`. To receive vendor events, a client subscribes to obs-websocket's `Vendors` event category.

Every event's `data` has `display`, `elapsed` and `mode` (as in the state), plus:

| Event | When | Extra data |
|---|---|---|
| `started` | The timer starts | — |
| `paused` | The timer pauses | `reason`: `marker` for a marker pause ("Pause timer on Marker"); `zero` when a Count Up turned down with "Prevent Negative Time Value" reaches 0 (it stops as Pause does; Count Up never expires); otherwise `user` |
| `expired` | A Count Down runs out (only a Count Down expires) | `expire_event`: the Timer Expire Event setting: `select`, `TIMER END TEXT`, `Source List`, `Scene List`, `Auto List`, or the scene's name |
| `overtime` | Overtime starts (Timer Expire Event "Continue counting (overtime)") | — |
| `reset` | A Reset | — |
| `split` | A split is recorded | `index` (integer); `split` (the split's mark); `interval` (the running time since the previous split). Both are in the split text's format |
| `marker` | Marker A or B fires | `marker`: `a` or `b`; `time`: the marker time |

**How the events combine:**

- **One event for each stop:** exactly one of `paused`, `expired` or `reset`. Resetting a running timer gives only `reset`.
- **List expire events:** a Count Down with a Source List, Scene List or Auto List expire event restarts itself (with two or more list entries and a start time above 0), so it gives `expired`, `reset`, `started`.
- **A Count Down with a start time of 0:** a Reset while it runs gives `expired`, then `reset` (it reaches 0 before it stops; the timer itself does the same without the bridge). Afterwards it isn't expired.
- **A timing setting changed** (mode, countdown type, date, time or duration) resets the timer, so it gives `reset`. Other settings changes don't.
- **No events** while Disable Script is ticked, and none while OBS is still loading: there is no `reset` event at start-up.

## Two copies of the script

Each copy of the script needs its own owner, otherwise only one of them reaches the dock.

- **The first copy:** leave **Instance ID** empty. Its owner is `stopwatch` and its section is called StopWatch.
- **Every other copy:** set an Instance ID, for example `2`. Its owner becomes `stopwatch.2` and its section is called StopWatch 2. Send its commands to `stopwatch.2`.

**How the ID is cleaned:**
- upper case becomes lower case;
- only `a`–`z`, `0`–`9`, `_` and `-` are kept;
- the owner is at most 64 characters, so the ID at most 54;
- an ID with nothing left after cleaning counts as empty.

**Two copies with the same ID** (both empty, for example) share an owner. Only one appears in the dock, and the OBS log says the owner "was already registered and active". Give one of them an ID.

## Compatibility with the Lua Bridge StopWatch contract

The Lua Bridge plugin ships a demo script, `stopwatch-demo.lua`, with the same owner `stopwatch`. **Don't run both:** they would share the owner. StopWatch 6.2 implements the demo's contract, so clients written for the demo work with it:

- the commands `toggle`, `start`, `pause`, `reset`, `add` and `subtract`;
- the state keys `display`, `elapsed`, `running` and `toggle_label`.

The differences come from StopWatch being a real timer:

- **`reset`** goes back to the start time: 0 in Count Up, the set time in Count Down;
- **`elapsed`** is the time remaining in Count Down;
- **`subtract`** stops at 0, unless negative time is allowed;
- **`add` and `subtract` without `seconds`** do nothing.

Everything else on this page is StopWatch's own extension.
