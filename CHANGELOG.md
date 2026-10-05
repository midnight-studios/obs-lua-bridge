# Changelog

All notable changes to Lua Bridge for OBS. Versions follow
[semantic versioning](https://semver.org/); the script API has its own
`api_version` (currently 1), see `docs/API.md`.

## Unreleased: 0.9.0 (first public beta)

### Added since M6
- **New example `lua/examples/ping-pong/`:** two scripts talking through the bridge. `ping.lua` sends commands, and
  `pong.lua` answers with events matched by the ping number. It handles pong being missing or reloaded, and
  avoids command loops.
- **Docs:** `run_command` from Lua is fire-and-forget; replies come back as events.

### Ideas for v1.1 (not implemented)
- A Lua request/reply helper, or a read-only `get_state(owner)` procedure, for script-to-script replies.

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
- **Examples:** `hello-bridge.lua`, `scoreboard.lua`, `stopwatch-demo.lua`. The examples have an optional
  Instance ID for running several copies.
- **License:** `lua/` is MIT, so the helper can be copied into scripts under any license. The plugin is
  GPL-2.0-or-later.

### Packaging and CI
- **Packages:**
  - Windows zip;
  - macOS `.pkg` and `.tar.xz` (unsigned community build);
  - Ubuntu `.deb` and `.tar.xz`;
  - a Lua-only zip.

  Every package ships `lua/` (helper, examples, README) and the license files.
- **Unit tests in CI:** the C++ tests on Windows, macOS and Ubuntu; the Lua tests on Windows and Ubuntu.
- **Releases:** a version tag creates a draft GitHub release.
- **OBS canary:** a weekly build against the newest OBS release.

### Development milestones
M0 setup · M1 core API · M2 obs-websocket vendor · M3 dock · M4 helper and examples · M5 hardening ·
M6 packaging and CI.
