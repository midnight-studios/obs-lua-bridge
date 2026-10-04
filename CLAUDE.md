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
