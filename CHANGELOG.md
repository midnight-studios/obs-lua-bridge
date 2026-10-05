# Changelog

All notable changes to Lua Bridge for OBS. Versions follow
[semantic versioning](https://semver.org/); the script API has its own
`api_version` (currently 1), see `docs/API.md`.

## Unreleased
- **Docs:** INSTALL.md has a FAQ: the dock starts hidden (Docks → Lua Bridge), and the "Legacy" label in OBS 33's
  Plugin Manager.

### Planned for 0.9.0-beta2
- Strip the debug symbols (`obs-lua-bridge.plugin.dSYM`) from the macOS `.pkg`. In Release builds the template
  installs them next to the plugin; they're harmless, but they clutter users' plugin folders and make the
  installer about 4.8 MB instead of about 0.35 MB.

### Ideas for v1.1 (not implemented)
- A Lua request/reply helper, or a read-only `get_state(owner)` procedure, for script-to-script replies.
- macOS signing and notarization.

## 0.9.0-beta1 (first public beta; date set when tagged)
**Requires OBS Studio 31.1 or newer.**
- **Platforms:** Windows x64, macOS universal (unsigned community build), Ubuntu 24.04 x86_64. Other Linux
  distributions via the tarball.
- **Version:** the plugin reports `0.9.0-beta1` (`luabridge_get_info`, websocket `GetInfo`, OBS log). The
  script API is `api_version` 1.

### Plugin
- **Script API** (`api_version` 1):
  - procedures: `luabridge_get_info`, `register`, `unregister`, `set_state`, `emit`, `heartbeat`, `run_command`;
  - signals: `luabridge_command`, `luabridge_event`;
  - all signals are delivered asynchronously on the UI thread, in order.
- **Lua Bridge dock:** one section per registered script. Controls: label, button (with confirm, `args_from`
  and `label_bind`), row, number, text, toggle and separator. Stale scripts are greyed out and can be removed.
- **obs-websocket vendor `LuaBridge`:** `GetInfo`, `ListOwners`, `ListCommands`, `GetState`, `RunCommand`;
  state changes and custom events are forwarded to clients.
- **Hardening:**
  - rate limiting for websocket `RunCommand` (30/s per owner, 200/s overall);
  - deduplicated warnings;
  - register and unregister are logged at debug level;
  - names are sanitized in log lines;
  - the confirm dialog never blocks OBS from closing.

### Lua helper (`lua/luabridge.lua`, helper version 1.0.0) and examples
- **The helper:** JSON, heartbeats, re-registration after removal, and a silent fallback when the plugin isn't
  installed. It never logs a warning in normal use.
- **Examples:**
  - `hello-bridge.lua`, `scoreboard.lua` and `stopwatch-demo.lua`, with an optional Instance ID for running
    several copies;
  - `ping-pong/` (`ping.lua` + `pong.lua`): two scripts talking through the bridge, with commands one way and
    events back, matched by the ping number.
- **Docs:** `run_command` from Lua is fire-and-forget; replies come back as events.
- **License:** `lua/` is MIT, so the helper can be copied into scripts under any license. The plugin is
  GPL-2.0-or-later.

### Packaging, CI and docs
- **Packages:**
  - Windows zip;
  - macOS `.pkg` (installs for the current user) and `.tar.xz`;
  - Ubuntu `.deb` and `.tar.xz`;
  - a Lua-only zip;
  - the source tarball.

  Every package ships `lua/` (helper, examples, README) and the license files.
- **Unit tests in CI:** the C++ tests on Windows, macOS and Ubuntu; the Lua tests on Windows and Ubuntu.
- **Releases:** a version tag creates a draft GitHub release. The tag must equal `buildspec.json`'s version.
- **OBS canary:** a weekly build against the newest OBS release, plus Dependabot for GitHub Actions.
- **Docs:** README for users, `docs/INSTALL.md`, control-surface research
  (`docs/integrations/control-surfaces.md`), `CONTRIBUTING.md`, `SECURITY.md`, issue templates.

### Development milestones
M0 setup · M1 core API · M2 obs-websocket vendor · M3 dock · M4 helper and examples · M5 hardening ·
M6 packaging and CI · ping-pong example · M7 launch prep.
