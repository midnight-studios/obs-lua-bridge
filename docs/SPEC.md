# Spec Guide: Lua Bridge for OBS — From Scratch to First Production Release

**Working name:** Lua Bridge for OBS (`obs-lua-bridge`). Rename freely; the name only appears in `buildspec.json`, the procedure and signal prefixes, and the websocket vendor name.
**Author:** GrumpyDog (midnight-studios)
**Goal of v1.0:** A small native OBS plugin that gives ordinary Lua scripts (running in Tools → Scripts) three new abilities:
- a dock with live controls;
- obs-websocket commands and events;
- a messaging channel to other scripts, plugins, and tools.

Scripts must keep working when the plugin isn't installed.

**Assumptions**
- Your development machine is **Windows 11**, freshly reset.
- macOS and Linux builds are produced by GitHub Actions. You do not need those machines to ship v1.0.
- Claude Code does the bulk of the coding. You review, run OBS, and do the manual testing.

---

## Part A — Machine setup (fresh Windows install)

### A1. Accounts you need

| Account | Why | Notes |
|---|---|---|
| **Claude Pro or Max plan**, or an Anthropic Console account with API billing | To run Claude Code | The free Claude.ai plan does not include Claude Code access |
| **GitHub** | Repository, Actions CI builds, releases | Enable 2FA. The plugin template's CI builds installers when you push a version tag |
| **OBS Forum** | Publishing the resource | You already have this |
| *(Later, optional)* **Apple Developer Program** | Signing and notarizing the macOS build | Paid yearly. You can postpone this; see Part F |

### A2. Tools to install

Open **Windows Terminal** as administrator and install the tools below. All of them except Claude Code use `winget`, which ships with Windows 11.

| # | Tool | Why it's needed | Install command |
|---|---|---|---|
| 1 | **Git for Windows** | Version control. Recommended for Claude Code on Windows (provides Git Bash) | `winget install --id Git.Git -e` |
| 2 | **PowerShell 7** | The plugin template's Windows build scripts require PowerShell 7.2+ | `winget install --id Microsoft.PowerShell -e` |
| 3 | **Visual Studio 2022 Community** with *Desktop development with C++* | Compiler, Windows SDK, debugger. The template targets the "Visual Studio 17 2022" generator | `winget install --id Microsoft.VisualStudio.2022.Community -e --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive"` |
| 4 | **CMake 3.30.5 or newer** | Build system. The template specifies 3.30.5 on Windows and macOS (3.28 minimum) | `winget install --id Kitware.CMake -e` |
| 5 | **GitHub CLI** | Lets Claude Code create the repo, open PRs and check CI runs | `winget install --id GitHub.cli -e` |
| 6 | **Python 3.12** | Formatting tools and websocket test scripts | `winget install --id Python.Python.3.12 -e` |
| 7 | **OBS Studio (current release)** | Running and testing the plugin | `winget install --id OBSProject.OBSStudio -e` |
| 8 | **VS Code** *(recommended)* | Reviewing Claude Code's changes; Lua and C++ editing | `winget install --id Microsoft.VisualStudioCode -e` |
| 9 | **Claude Code** | The coding agent | `irm https://claude.ai/install.ps1 \| iex` (in PowerShell) |

After the winget installs, **close and reopen the terminal** so PATH updates apply. Then install the Python tools:

```powershell
py -m pip install --upgrade pip
py -m pip install "clang-format>=19,<20" gersemi obsws-python
```

- **clang-format 19** and **gersemi** match the template's CI format checks (clang-format 19 for C/C++, gersemi for CMake). Formatting locally avoids failed CI runs.
- **obsws-python** is a websocket client for automated tests of the vendor requests (Milestone 2).

**Optional extras**
- VS Code extensions: *C/C++*, *CMake Tools*, *Lua (sumneko)*, and the *Claude Code* extension.
- A second, **portable OBS** install used only for testing. Download the ZIP from obsproject.com and create an empty `portable_mode.txt` next to `obs64.exe`. This keeps test scenes and crashes away from your streaming setup.

### A3. Verify the toolchain

Run each command in a **new** PowerShell 7 window (`pwsh`):

```powershell
git --version          # any recent version
pwsh -v                # 7.2 or newer
cmake --version        # 3.30.5 or newer
gh --version
py --version           # 3.12.x
clang-format --version # 19.x
gersemi --version
claude --version
claude doctor          # Claude Code's own health check
```

Visual Studio check: open **Developer PowerShell for VS 2022** and run `cl`. It should print the MSVC compiler banner.

### A4. First-time logins

```powershell
gh auth login          # GitHub.com → HTTPS → login with browser
claude                 # follow the login prompt (Claude plan or Console account)
git config --global user.name  "GrumpyDog"
git config --global user.email "<your GitHub email>"
```

---

## Part B — Product specification

> **Exact request/response shapes:** [docs/API.md](API.md) is the authoritative reference. This part describes intent, and where the two differ, API.md wins.

### B1. Goals for v1.0
1. Scripts can register **commands** that are reachable from a dock button and from obs-websocket.
2. Scripts can **publish state** (key/value pairs) that the dock displays and websocket clients can read.
3. Scripts receive **commands as signals** and can emit **custom events**.
4. Works on **Windows, macOS, and Linux**, OBS **31.0 or newer**.
5. **Zero impact** on scripts or OBS when the plugin is absent or a script misbehaves.

### B2. Non-goals for v1.0 (candidates for later versions)
- A separate Lua runtime or script host.
- HTTP or network access for scripts.
- Fully custom Qt UI per script. v1.0 offers declarative controls only.
- Python script support. Python scripts can use the same API in principle, but test that in v1.1.

### B3. Architecture

```
   Lua script (Tools → Scripts)                 Lua Bridge plugin (C++)
   ─────────────────────────────                ────────────────────────────────
   proc_handler_call("luabridge_*")  ───────▶   Procedures on the global proc handler
                                                  • registry of owners/commands/state
   signal_handler_connect(           ◀───────   Signals on the global signal handler
     "luabridge_command")                         • emitted on the UI thread
                                                 Dock (Qt, obs_frontend_add_dock_by_id)
                                                 obs-websocket vendor "LuaBridge"
                                                   • requests → signals to scripts
                                                   • state/events → websocket events
```

This design is confirmed against OBS source:
- Lua scripts can call `proc_handler_call`. The Lua bindings include `callback/proc.h` and `callback/calldata.h`.
- Lua scripts can receive custom signals. OBS's Lua `signal_handler_connect` accepts any declared signal name.
- Scripts **cannot register procedures**. Plugin → script traffic is therefore signals only.
- A signal must be **declared before** a script connects to it. The plugin declares all signals in `obs_module_load`, which runs before any script loads.

### B4. Data rules
- All structured data crosses the boundary as **UTF-8 JSON strings** in calldata, because pointer types are unreliable from Lua.
- Owner IDs: `[a-z0-9_.-]{1,64}`, for example `stopwatch` or `grumpydog.scoreboard`. Scripts duplicated for multiple instances append a suffix.
- Command IDs: `[a-z0-9_]{1,64}`. State keys: `[a-zA-Z0-9_.]{1,64}`.
- Size limits: JSON payload ≤ 64 KB; ≤ 64 commands and ≤ 256 state keys per owner; ≤ 128 owners. Calls that exceed a limit return an error string. They do not crash.

### B5. API contract — procedures

Registered on `obs_get_proc_handler()`. Every procedure returns `out bool ok` and `out string error`.

| Procedure | Inputs | Outputs | Purpose |
|---|---|---|---|
| `luabridge_get_info` | — | `string json` | `{api_version, plugin_version, obs_version, capabilities}`. `capabilities` is an object of flags, e.g. `{"commands":true,…,"websocket":false}`, because obs-websocket can't carry arrays of strings. If this call returns false, the plugin is absent and the script falls back |
| `luabridge_register` | `string owner`, `string json` | `ok`, `error` | Declares the display name, commands, and dock controls (B7). Calling it again replaces the previous registration |
| `luabridge_unregister` | `string owner` | `ok`, `error` | Removes everything for this owner. Scripts call it from `script_unload` |
| `luabridge_set_state` | `string owner`, `string json` | `ok`, `error` | Merges key/values into the owner's state. Values are string, number or boolean; `null` deletes a key. Updates the dock and emits the websocket `StateChanged` event |
| `luabridge_emit` | `string owner`, `string event`, `string json` | `ok`, `error` | Sends a custom event to websocket clients and other scripts. The data can't contain `null` or arrays of non-objects, which obs-websocket would silently drop; such calls are rejected with an error |
| `luabridge_heartbeat` | `string owner` | `ok`, `error` | Optional liveness ping (B9) |
| `luabridge_run_command` | `string owner`, `string command`, `string json` | `ok`, `error` | Invokes another owner's command; the target receives `luabridge_command` with `origin = "script"` |

Declaration example (C++):

```cpp
proc_handler_add(ph,
  "void luabridge_register(in string owner, in string json, out bool ok, out string error)",
  proc_register, this);
```

### B6. API contract — signals

Declared on `obs_get_signal_handler()` in `obs_module_load`:

| Signal | Parameters | Sent when |
|---|---|---|
| `luabridge_command` | `string owner, string command, string json, string origin` | A dock button, websocket request, or another script invokes a command. `origin` is `dock`, `websocket`, or `script` |
| `luabridge_event` | `string owner, string event, string json` | Any owner calls `luabridge_emit`. This lets scripts listen to each other |
| `luabridge_ready` | `string json` (info) | The plugin finished loading, and again on `OBS_FRONTEND_EVENT_FINISHED_LOADING` |

**Threading rule:** All signals to scripts are delivered **asynchronously** on the **UI thread**, after the call that caused them returns, in **FIFO order**.
- Procedures can be called from any thread: the UI thread, the graphics thread (Lua timers) or obs-websocket's worker threads. Every emit is therefore posted with `QMetaObject::invokeMethod(…, Qt::QueuedConnection)` to a `QObject` the plugin owns.
- `obs_queue_task(OBS_TASK_UI, …)` is **not** used: when called on the UI thread it runs the task immediately, which would re-enter the calling script and break ordering.
- Nothing is emitted once shutdown begins. Deleting the plugin's `QObject` at unload discards emits that are still queued.

This keeps script callbacks safe and their order predictable.

### B7. Registration JSON (v1)

```json
{
  "display_name": "Stopwatch",
  "commands": [
    { "id": "start",  "label": "Start",  "description": "Start the timer" },
    { "id": "pause",  "label": "Pause" },
    { "id": "reset",  "label": "Reset",  "confirm": true },
    { "id": "add",    "label": "+",      "args": { "seconds": "int" } }
  ],
  "dock": [
    { "type": "label",  "bind": "display",   "style": "large" },
    { "type": "row",    "items": [
        { "type": "button", "command": "start" },
        { "type": "button", "command": "pause" },
        { "type": "button", "command": "reset" } ] },
    { "type": "number", "id": "add_secs", "min": 1, "max": 3600, "default": 1 },
    { "type": "button", "command": "add", "args_from": { "seconds": "add_secs" } },
    { "type": "separator" }
  ]
}
```

v1.0 control types: `label` (bound to a state key), `button`, `row`, `number`, `text`, `toggle` (bound to a state key; sends a command with `{ "value": true|false }`), and `separator`. Unknown types are skipped with a log warning, so newer scripts degrade gracefully on older plugins.

### B8. obs-websocket vendor API

Vendor name: **`LuaBridge`**. Register it in `obs_module_post_load`, because obs-websocket must already be loaded. If obs-websocket is unavailable, log a message and continue without it.

Every response is an object with `"ok": true|false`. A failed response adds `"error"`, using the same strings as the script API. This is necessary because obs-websocket reports every vendor request as successful. Responses can't be bare arrays (they travel as `obs_data`), so lists are wrapped in an object.

| Request | Request data | Response |
|---|---|---|
| `GetInfo` | — | `{ok, api_version, plugin_version, obs_version, capabilities}`. Same fields as `luabridge_get_info` |
| `ListOwners` | — | `{ok, owners: [{owner, display_name, stale}]}` |
| `ListCommands` | `{owner}` | `{ok, owner, commands: [{id, label, description, confirm, args}]}` |
| `RunCommand` | `{owner, command, data}` | `{ok, accepted: true}`. This is fire-and-forget: the script reports results through state or events |
| `GetState` | `{owner}` | `{ok, owner, state}` |

| Event | Data |
|---|---|
| `StateChanged` | `{owner, changes, removed?}`. `changes` holds new and updated values. `removed` (`{key: true}`) lists deleted keys, because obs-websocket can't carry `null` |
| `CustomEvent` | `{owner, event, data}` |

Security note: obs-websocket's own authentication protects these requests. The bridge exposes only commands that scripts explicitly register, and all input is validated against B4.

### B9. Lifecycle and robustness
- **Script unload:** Scripts call `luabridge_unregister` in `script_unload`. Scripts can crash or be removed without unloading cleanly, so owners with a heartbeat that goes silent for 30 s are greyed out in the dock but kept. Their commands are ignored until they register again.
- **Script reload:** Registering again with the same owner replaces the old entry atomically.
- **Plugin unload / OBS exit:** Remove the dock and free all registries. Do not emit signals during shutdown (`OBS_FRONTEND_EVENT_EXIT`).
- **Errors:** Never throw across the C boundary. Log with the `[lua-bridge]` prefix, and return `ok=false` plus a short message.
- **Persistence:** None in v1.0. Scripts re-register on load. OBS persists the dock's position automatically.

### B10. Lua helper library (`luabridge.lua`)

Ship a single-file helper that script authors copy next to their script and load with `dofile(script_path() .. "luabridge.lua")`. It wraps the calldata boilerplate.

```lua
local bridge = dofile(script_path() .. "luabridge.lua")

function script_load(settings)
  if not bridge.available() then return end          -- plugin absent: script works as before
  bridge.register("stopwatch", REG_TABLE)             -- table is converted to JSON
  bridge.on_command("stopwatch", function(cmd, data, origin)
    if cmd == "reset" then reset(true) end
  end)
end

function script_unload() bridge.unregister("stopwatch") end
```

The helper includes a minimal pure-Lua JSON encoder/decoder, because OBS Lua has none built in.

---

## Part C — Repository setup

### C1. Create from the official template
1. On GitHub, open **obsproject/obs-plugintemplate** → **Use this template** → create `midnight-studios/obs-lua-bridge` (public).
2. Clone it: `gh repo clone midnight-studios/obs-lua-bridge` into `C:\dev\`.
3. Edit **`buildspec.json`**:
   - `name: "obs-lua-bridge"`, `displayName: "Lua Bridge for OBS"`, `version: "0.1.0"`, `author`, `website`, `email`.
   - Set a real macOS `bundleId`, for example `com.midnightstudios.obs-lua-bridge`.
   - Set `obs-studio.version` to your **minimum supported OBS version**. The template currently pins 31.1.1; 31.x is a sensible minimum. Update the hashes the way the template wiki describes.
4. In **`CMakeLists.txt`**, turn on `ENABLE_FRONTEND_API` and `ENABLE_QT` (both default to OFF). The dock needs both.
5. Switch the entry file from `src/plugin-main.c` to C++ (`src/plugin-main.cpp`). Qt and the registry code are C++17.
6. Vendor `obs-websocket-api.h` from `obsproject/obs-websocket/lib/` into `src/third-party/`. It is a single header with no linking required.
7. The template is GPL-2.0. Keep it; plugins that link libobs must be GPL-compatible.

### C2. Target layout

```
obs-lua-bridge/
├─ CLAUDE.md                 # instructions for Claude Code (C3)
├─ buildspec.json
├─ CMakeLists.txt
├─ src/
│  ├─ plugin-main.cpp        # module load/unload, signal declarations
│  ├─ registry.hpp/.cpp      # owners, commands, state, limits, validation
│  ├─ procs.cpp              # proc handler implementations
│  ├─ websocket-vendor.cpp   # vendor requests/events
│  ├─ dock/                  # Qt dock + declarative control renderer
│  └─ third-party/obs-websocket-api.h
├─ lua/
│  ├─ luabridge.lua          # helper library (B10)
│  └─ examples/              # hello-bridge.lua, stopwatch integration, scoreboard demo
├─ tests/
│  ├─ registry-tests.cpp     # pure C++ unit tests (no OBS needed)
│  ├─ websocket/test_vendor.py
│  └─ lua/test-harness.lua   # in-OBS smoke test script
├─ docs/  API.md, GETTING-STARTED.md, CHANGELOG.md
└─ .github/                  # template CI (build, format, release)
```

### C3. `CLAUDE.md`, the project brief for Claude Code

Create this first. Claude Code reads it at the start of every session.

```markdown
# Lua Bridge for OBS — project rules
- Spec: docs/SPEC.md (copy of this guide's Parts B, D, E). Do not change the public API
  (procedure/signal names, JSON shapes) without updating docs/API.md and asking first.
- Language: C++17 for plugin code, Lua 5.1/LuaJIT-compatible for lua/.
- Build (Windows): `cmake --preset windows-x64` then `cmake --build --preset windows-x64`.
- Format before every commit: `clang-format -i` on changed C/C++ files, `gersemi -i` on CMake files.
- All script-facing calls: validate input, never crash, return ok/error, log with "[lua-bridge]".
- Signals to scripts are emitted on the UI thread only (obs_queue_task(OBS_TASK_UI, ...)).
- Work in small steps: one milestone task per branch/PR; build must pass before you say "done".
- Never commit secrets, signing certificates, or files from the user's OBS profile.
```

### C4. Claude Code working agreement
- Start each milestone in **plan mode**: ask Claude Code to propose the change list, review it, then approve.
- One milestone task per git branch. Claude Code opens the PR with `gh pr create`, and CI must pass before you merge.
- Claude Code can build, run unit tests, and run the Python websocket tests against a running OBS. **You** do the visual checks: dock appearance, clicking buttons, watching the timer. Claude Code cannot see the OBS window.
- After each manual test, paste the relevant part of the OBS log (`Help → Log Files → View Current Log`) back into Claude Code when something fails.

---

## Part D — Milestones

Each milestone ends with a **tagged pre-release** (`0.x.0`) so CI produces installable artifacts you can test.

### M0 — Spike: prove the round trip (½ day)
**Build:** A hard-coded `luabridge_get_info` procedure and a `luabridge_command` signal, plus a temporary OBS **Tools menu** item that emits `luabridge_command("hello","ping","{}","dock")`.
**Lua:** `hello-bridge.lua` calls `get_info`, prints the result, connects to the signal, and logs when "ping" arrives.
**Done when:** On Windows OBS, the script log shows the info JSON at load and "ping received" after you click the menu item.

### M1 — Core bridge (2–3 days)
- Registry with all B4 limits and validation, and unit tests in `tests/registry-tests.cpp`.
- All B5 procedures and B6 signals. UI-thread emission.
- Heartbeat and stale-owner handling (B9).
- **Done when:** Unit tests pass, and the harness script registers, sets state, receives commands, unregisters, and re-registers 100× without leaks or warnings in the log.

### M2 — obs-websocket vendor (1–2 days)
- Vendor registration in `obs_module_post_load`, with all B8 requests and events.
- `tests/websocket/test_vendor.py` uses `obsws-python` to call each request and assert on the responses and events.
- **Done when:** The Python tests pass against a running OBS, and a Stream Deck (or any websocket tool) can trigger `RunCommand`.

### M3 — Dock (2–3 days)
- One **"Lua Bridge"** dock, added with `obs_frontend_add_dock_by_id` (OBS 30+ API), with one collapsible section per owner.
- A renderer for the B7 control types. Labels update live from state.
- Stale owners are shown greyed out. A "no scripts registered" placeholder shows when the dock is empty.
- **Done when:** The stopwatch example (M4) is fully controllable from the dock, and the dock survives script reload, owner removal, and an OBS restart.

### M4 — Lua helper and reference integrations (2 days)
- `luabridge.lua` (B10), with a JSON encoder/decoder and a test harness.
- **Example 1:** Stopwatch 5.10 integration. Start, Pause, Reset, Add/Subtract and a live display in the dock; websocket commands that don't depend on the script's filename.
- **Example 2:** a minimal Score Board demo (state → dock and websocket events).
- **Done when:** Both examples work with and without the plugin installed.

### M5 — Hardening (2 days)
- Fuzz the procedures from Lua with oversized, malformed, and unicode JSON and wrong types.
- Stress test: 10 owners, 1,000 commands per minute from websocket, for 1 hour. Watch memory in Task Manager.
- Clean shutdown: exit OBS while commands are in flight. No crash and no "signal not found" warnings.
- Test with Studio Mode on and off, with scene collection switches, and with multiple duplicated script instances.
- Rate limiting / flood protection for websocket `RunCommand`.
- Reduce the `[lua-bridge] registered owner '…'` / `unregistered owner '…'` log lines (`src/procs.cpp`) from info to debug level. They're useful during development, but users with many scripts would see them on every start and exit.

### M6 — Packaging and CI (1–2 days)
- The template workflows build Windows, macOS, and Ubuntu on every push and PR. Fix any platform build errors Claude Code can't reproduce locally by reading the CI logs (`gh run view --log-failed`).
- Pushing a semantic-version tag (for example `1.0.0`) creates a **draft GitHub release** with the platform artifacts attached.
- Windows: the template packages a ZIP. Write install instructions (copy into `C:\ProgramData\obs-studio\plugins\`), or add an installer later.
- macOS: see F2 for signing.

### M7 — Documentation and release (1–2 days)
- `docs/API.md` (the full contract from Part B), `docs/GETTING-STARTED.md` (a five-minute "add a dock to your script" guide), and `CHANGELOG.md`.
- README with screenshots of the dock and a short GIF.
- OBS Forum resource: overview, install steps per OS, a link to the API docs, and a link to the Stopwatch integration as a showcase.
- Before announcing, contact Exeldro and the OBS team (B1 of the strategy): share the repo, invite API feedback, and state openness to upstreaming.

**Rough total:** 2–3 weeks part-time, with Claude Code doing most of the implementation.

---

## Part E — Testing plan

| Layer | How | Who runs it |
|---|---|---|
| Registry logic | `tests/registry-tests.cpp`, built as a separate CMake test target, run with `ctest` | Claude Code, every change |
| Websocket API | `tests/websocket/test_vendor.py` against a running portable OBS | Claude Code, once you start OBS |
| Lua integration | `tests/lua/test-harness.lua`, loaded in Tools → Scripts; prints PASS/FAIL to the script log | You load it; Claude Code reads the pasted log |
| Dock and UI | Manual checklist (below) | You |
| Platforms | CI builds; one manual smoke test per OS before 1.0 (borrow a Mac/Linux machine or ask forum testers) | You and community |

**Manual dock checklist**
- ☐ The dock appears under Docks and remembers its position after a restart.
- ☐ Buttons trigger commands, and labels update.
- ☐ Toggles and number inputs work.
- ☐ A script reload replaces its section without duplicates.
- ☐ Removing a script greys out its section, then clears it once the script unregisters.
- ☐ Dark and light themes are readable.
- ☐ The dock works in Studio Mode.

---

## Part F — Production-readiness checklist (gate for 1.0.0)

### F1. Must have
- ☐ All milestones' acceptance criteria are met.
- ☐ No crashes or warnings in the OBS log during the M5 tests.
- ☐ Builds pass on Windows, macOS, and Linux via CI. Smoke tested on at least Windows plus one other OS.
- ☐ Minimum OBS version is stated and tested (31.x) on the current OBS release too.
- ☐ The API is documented and frozen as `api_version = 1`. Future changes are additive only.
- ☐ Scripts without the plugin behave exactly as before.
- ☐ GPL-2.0 license, third-party notice for `obs-websocket-api.h`, and a changelog.
- ☐ Issue templates on GitHub (bug report asks for the OBS version, OS, and log file).

### F2. macOS signing decision
Unsigned macOS plugins can be blocked by Gatekeeper and need manual approval. There are two options:
- **(a)** Join the Apple Developer Program and use the template's signing and notarization setup for GitHub Actions. The template wiki has a dedicated guide.
- **(b)** Ship macOS as a "community build" with documented manual approval steps, and sign it in 1.1.

Option (b) is acceptable for a first release if it's stated clearly.

### F3. Should have
- ☐ A second contributor with merge rights, or at least a documented release process in `docs/RELEASING.md`.
- ☐ Dependabot or a monthly reminder to bump `buildspec.json` when OBS releases a new version.

---

## Part G — Risks and open decisions

| Item | Decision needed | Recommendation |
|---|---|---|
| Plugin name and prefix | Before M0 | Keep `luabridge_` short and unique. Check the forum for name collisions |
| Minimum OBS version | Before M1 | 31.0. The dock API needs 30+, and 31 matches the template |
| Dock model | Before M3 | One shared dock with sections per script (v1.0). Per-script docks in v1.1 if requested |
| Network access for scripts | After 1.0 | High demand but a security responsibility. Design it separately (allow-list, user consent) |
| Upstreaming | After adoption | Share usage evidence with the OBS team once several third-party scripts use the bridge |
| OBS updates breaking the build | Ongoing | CI on every OBS release. The small API surface keeps fixes cheap |

---

## Appendix — Starter prompts for Claude Code

1. **Setup:** "Read CLAUDE.md and docs/SPEC.md. Configure the template for this project as described in Part C1 (buildspec, CMake options, C++ entry point, vendored websocket header). Build with the windows-x64 preset and fix any errors. Don't implement features yet."
2. **M0:** "Implement Milestone M0 exactly as specified. Also write lua/examples/hello-bridge.lua. Tell me what to click in OBS and what log lines to expect."
3. **M1:** "Plan Milestone M1 in plan mode first: list files, data structures, and test cases. Wait for my approval."
4. **Per failure:** "Here is the OBS log excerpt from my test: [paste]. Diagnose the cause before changing code."
5. **Release:** "Prepare release 1.0.0: update the changelog, verify the Part F checklist items you can check from the repo, and list the ones I must verify manually."
