# Contributing to Lua Bridge for OBS

Thanks for helping! Bug reports and ideas go in
[issues](https://github.com/midnight-studios/obs-lua-bridge/issues); security problems go through
[SECURITY.md](SECURITY.md), not public issues.

## Building
The plugin uses the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate) build system;
see its [wiki](https://github.com/obsproject/obs-plugintemplate/wiki) for the toolchain each platform needs.
- **Windows:** Visual Studio 2022 or newer, CMake 3.28+.
  ```
  cmake --preset windows-x64
  cmake --build --preset windows-x64
  ```
- **macOS / Ubuntu:** the presets `macos` and `ubuntu-x86_64`.
- **The first configure** downloads the OBS sources and dependencies pinned in `buildspec.json`.
- **The version** lives in one place, `"version"` in `buildspec.json` (e.g. `0.9.0-beta1`):
  - the plugin reports it in `luabridge_get_info` and the OBS log, and the packages are named with it;
  - only its numeric part (`0.9.0`) goes into the Windows/macOS file version fields;
  - the script API has its own integer `api_version`.

To try a build, deploy it into a portable test OBS (never your everyday one) with `tools/deploy-test.ps1`.

## Code layout
- `src/`:
  - the plugin: procedures (`procs.cpp`), signals, the dock (`dock/`) and the obs-websocket vendor;
  - the pure-C++ core (registry, events, rate limiter, …), unit-tested without OBS.
- `lua/`: the helper `luabridge.lua` and the examples. MIT-licensed; keep them Lua 5.1/LuaJIT compatible.
- `tests/`: the C++ and Lua unit tests, the in-OBS harnesses and the websocket suites.
- `docs/`: [API.md](docs/API.md) (the public contract), [TESTING.md](docs/TESTING.md), [RELEASING.md](docs/RELEASING.md),
  and [SPEC.md](docs/SPEC.md) (the original design).

## Rules
- **The public API is a contract:**
  - procedure and signal names, the JSON shapes, and the websocket requests;
  - change it only additively, and update `docs/API.md` in the same PR;
  - a breaking change needs a new `api_version` and a discussion first.
- **Script-facing calls** validate their input, never crash, return `ok`/`error`, and log with the
  `[lua-bridge]` prefix.
- **Signals to scripts** are delivered asynchronously on the UI thread, in order, through the `Emitter`
  interface (`src/emitter.hpp`), never inline.
- **Formatting:** run before every commit; CI checks it.
  - `clang-format -i` (version 19) on changed C/C++ files;
  - `gersemi -i` on changed CMake files;
  - `tools/format.ps1` does both.

  The gersemi CI uses comes from `obsproject/tools`; newer local versions may reformat template files you
  didn't touch, so don't commit those.
- **Never commit** secrets, signing certificates, or files from your OBS profile.

## Tests
- `py tests/lua-unit/test_luabridge.py`: the Lua helper and examples (needs `py -m pip install lupa`).
- `ctest --test-dir build_x64 -C RelWithDebInfo`: the C++ unit tests (configured by the `windows-x64` preset).
- **The in-OBS suites** (websocket, fuzzing, stress, shutdown, switching, ping-pong) and what needs a person:
  [docs/TESTING.md](docs/TESTING.md).

CI runs the unit tests on Windows, macOS and Ubuntu, builds all three platforms, and checks formatting.

## Pull requests
- **One topic per branch and PR,** with a description of what changed and how it was tested. All CI checks must
  be green before merging.
- **Installable packages:** add the label **Seeking Testers** to a PR and its CI builds them (Windows zip, macOS
  pkg, Ubuntu deb, plus archives) as run artifacts.
- **Releases:** [docs/RELEASING.md](docs/RELEASING.md).

## License of contributions
- **Plugin code** (`src/`, build files): contributed under **GPL-2.0-or-later**.
- **`lua/`:** contributed under **MIT**.

Keep the license headers in the files you change.
