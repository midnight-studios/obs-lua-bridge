<!-- DRAFT: not sent. Short note to the OBS team (e.g. the OBS Discord #plugins-and-tools channel or a maintainer). Replace {{placeholders}}. -->

Hi OBS team,

A short heads-up about a plugin we're releasing as a public beta: **Lua Bridge for OBS** ({{GitHub repository link}}).

**What it is.** It gives Lua scripts things the scripting API doesn't offer today: a shared dock with controls
(buttons, labels, inputs, toggles), commands that other scripts and obs-websocket clients can run, and
published state plus custom events.

**How it fits in:**
- **For scripts:** a handful of procedures on the global proc handler (`luabridge_register`, `set_state`,
  `emit`, `run_command`, …) and two signals (`luabridge_command`, `luabridge_event`), delivered asynchronously
  on the UI thread.
- **For websocket clients:** an obs-websocket vendor `LuaBridge`, with `GetInfo`, `ListOwners`,
  `ListCommands`, `GetState` and `RunCommand` (rate limited), plus vendor events.
- **One dock** via `obs_frontend_add_dock_by_id`, removed at `OBS_FRONTEND_EVENT_EXIT`.
- **The full contract:** {{docs/API.md link}}.

**Status:** GPL-2.0-or-later, built from the official plugin template. It's tested with fuzzing, stress and
soak runs, and shutdown with work in flight. Requires OBS 31.1+; tested on 32.2.2.

We'd welcome any feedback on the approach. If parts of it would ever be useful in OBS itself, for example a
scripting-side dock or command API, we'd be glad to discuss upstreaming.

Thanks for OBS and for obs-websocket's vendor API, which made this possible.

{{name}}, Midnight Studios
