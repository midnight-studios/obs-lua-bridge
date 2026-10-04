# Testing Lua Bridge for OBS

Every test suite, how to run it, how long it takes, and what needs a person.

**In CI** (`.github/workflows/tests.yaml`, on every PR and push):
- **C++ unit tests:** on Windows, macOS and Ubuntu. `tests/CMakeLists.txt` is a standalone build of the core
  library that needs no OBS: `cmake -S tests -B build_tests && cmake --build build_tests --config Release &&
  ctest --test-dir build_tests -C Release`.
- **Lua unit tests:** on Windows and Ubuntu. They don't run on macOS because lupa's Apple Silicon wheel has no
  LuaJIT.

Everything else below runs locally against a test OBS.

**Install test of a Windows package without touching ProgramData:**
- A portable OBS ignores `C:\ProgramData\obs-studio\plugins`. It still reads `OBS_PLUGINS_PATH`, the plugin's
  `bin\64bit` folder, and `OBS_PLUGINS_DATA_PATH`, a folder containing `obs-lua-bridge` as the package's `data`
  folder, for example through a junction.
- So the package can be tested unmodified in a clean portable OBS, with the same `bin/64bit` + `data` split
  that ProgramData uses.
- A fresh portable OBS reports `Number of memory leaks: 1` on its very first start, with or without plugins.
  Judge leaks from the second start on.

## The test OBS

All in-OBS tests run against a **portable OBS** in `C:\obs-test`, never your
everyday OBS. Its WebSocket server must be enabled. The Python tests read the
connection from the environment only:

| Variable | Default |
|---|---|
| `OBS_WS_HOST` | `localhost` |
| `OBS_WS_PORT` | `4455` |
| `OBS_WS_PASSWORD` | unset (no authentication) |

Python requirements: `py -m pip install obsws-python psutil lupa`.

`py tests/websocket/setup_test_obs.py --backup <dir>` (OBS closed) backs up the
test OBS's `basic` config, then creates the extra scene collections and the
profile that the M5 tests use: "LuaBridge B", "LuaBridge Dup",
"LuaBridge Conflict", "LuaBridge Fuzz" and the profile "LuaBridge P2". It is
idempotent.

**Closing the test OBS from a script:** use `tools/close-test-obs.ps1`. It
sends the same message as the window's close button and never kills OBS. Don't
use .NET `Process.CloseMainWindow()`. When a Lua script logs a warning, OBS
opens its **Script Log** window, and .NET then reports *that* window as the main
window. `CloseMainWindow()` then closes only the Script Log, and OBS looks as if
it refused to close. `tools/obs-windows.ps1` lists the visible windows of a
running OBS, to see what is open.

Stop your own websocket clients (watchers, test scripts) before closing the
test OBS. A client that keeps reconnecting while OBS shuts down holds up
obs-websocket's unload; the OBS window is gone but the process lingers until
the client stops.

## Suites

| Suite | Command | Needs | Time |
|---|---|---|---|
| Core unit tests (registry, events, dock logic, rate limiter, log limiter) | `ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure` | build only | seconds |
| Lua helper and example unit tests (LuaJIT 2.1 via lupa); includes "the examples log no warning, with or without the plugin" | `py tests/lua-unit/test_luabridge.py` | nothing | seconds |
| Raw API and helper harnesses | load `tests/lua/test-harness.lua` and `tests/lua/helper-harness.lua` in Tools > Scripts; each prints `RESULT: n passed, 0 failed` to the log | test OBS | seconds |
| Dock | `tests/lua/dock-test.lua` | test OBS, a person looking at the dock | 5 min |
| Websocket API | `py tests/websocket/test_vendor.py` | `ws-companion.lua` loaded | 30 s |
| Examples | `py tests/websocket/test_examples.py` | `scoreboard.lua`, `stopwatch-demo.lua` loaded | 30 s |
| Rate limiting | `py tests/websocket/test_rate_limit.py` | `ws-companion.lua`, `hello-bridge.lua` loaded | 15 s |
| Ping-pong example | `py tests/websocket/test_ping_pong.py`, then `--close` | the "LuaBridge PingPong" and "LuaBridge Ping Only" collections (`setup_test_obs.py`) | 1 min |

**Ping-pong checks** (`test_ping_pong.py`). It switches collections by itself and returns to the one it started in.
1. **ping without pong:**
   - ping shows "pong not loaded", and the ping number isn't used up;
   - 20 sends produce 1–2 `owner not registered` log lines (deduplicated).
2. **Round trip:** 50 sends at 5/s give pong's `count` 50, and ping shows `Last pong: #50`.
3. **Rapid clicking:**
   - 300 sends as fast as possible. The websocket rate limit rejects the excess, and every accepted send gives exactly one pong.
   - **Pass rule:** `GetInfo` keeps answering within 1 s. p99 latency is reported only.
4. **`--close`:**
   - closes the test OBS while ping and pong are busy. Same pass rules as shutdown-in-flight.
   - It restores the current collection in `user.ini` afterwards.

### Hardening (M5)

| Suite | Command | Time |
|---|---|---|
| Core fuzzing (libFuzzer + AddressSanitizer, MSVC) | see below | 15 min |
| Websocket fuzzing: all five vendor requests, 5,000 payloads each | `py tests/websocket/fuzz_vendor.py` | 6 min |
| Procedure fuzzing from Lua: 10,000 calls | switch the test OBS to the "LuaBridge Fuzz" collection, then back; the log shows `RESULT` | 1 min |
| Stress: 10 owners, 1,000 commands/min, Stopwatch running | `py tests/websocket/stress.py` (needs `tests/lua/stress-owners.lua` and `stopwatch-demo.lua` loaded) | 60 min |
| Overnight soak | `py tests/websocket/stress.py --minutes 480 --rate 6 --sample 300 --window-start 60` | 8 h |
| Shutdown with work in flight | `py tests/websocket/shutdown_in_flight.py --cycles 10` (OBS closed at start) | 15 min |
| Scene collection, profile and Studio Mode switching | `py tests/websocket/switching.py` | 20 min |
| Duplicated scripts with instance IDs | `py tests/websocket/duplicates.py` | 2 min |

`fuzz_vendor.py` options:
- `--requests RunCommand` fuzzes one request type.
- `--vendor NoSuchVendor` sends the same traffic without it reaching the plugin, as a control.
- `--seed` reproduces a run.

**Core fuzzing.** Configure a separate build directory with fuzzing on:

```powershell
cmake --preset windows-x64 -B build_fuzz -DENABLE_FUZZING=ON -DENABLE_TESTS=OFF
cmake --build build_fuzz --config RelWithDebInfo --target registry-fuzz
# the ASan runtime DLL lives next to the MSVC compiler
$env:PATH = "<MSVC>\bin\Hostx64\x64;$env:PATH"
mkdir corpus, artifacts
build_fuzz\RelWithDebInfo\registry-fuzz.exe -max_total_time=900 -rss_limit_mb=2048 `
    -artifact_prefix=artifacts\ corpus tests\fuzz\corpus
```

`tests/fuzz/make_corpus.py` regenerates the seed corpus. LeakSanitizer isn't
available on Windows. Leaks are covered by OBS's own `Number of memory leaks`
line at exit and by the memory checks of the stress test.

**Pass criteria** are in each script's docstring. In short:
- no crash, no hang, no malformed response;
- `Number of memory leaks: 0` at exit;
- memory reaches a plateau (less than 5 % growth after warm-up);
- no `[lua-bridge]` errors (`could not send`, `unexpected exception`) in the log.

Memory note: the first websocket fuzz round raises OBS's private memory by about
85 MB (1 MB payloads raise the heap's high-water mark). Repeating the fuzz in
the same session doesn't raise it further.

## What needs a person

These need someone at the OBS window, about 15 minutes in all:
- **Dock look and feel** (`dock-test.lua`).
- **Collection switching with the dock open, then closed:** switch collections
  about five times by hand. The dock must show exactly the current collection's
  sections, with no duplicates and no stale ones.
- **Duplicates:** in "LuaBridge Dup", the dock shows two separate Stopwatch and
  two separate Score Board sections that don't affect each other.
- **Confirm dialogs at exit:** open a dock confirm dialog (Score Board → Reset),
  leave it open, and close OBS. OBS must close, and the dialog with it.
- **Ping-pong: reloading and removing pong** (2 min, in "LuaBridge PingPong" with
  the dock open):
  1. Click **Send ping** a few times, then in Tools → Scripts select `pong.lua` →
     **Reload**, and click again.
     - **Pass:** ping continues with the next number;
     - pong's section shows "Pongs: 1" again (its counter restarts);
     - no error appears in the Script Log.
  2. Remove `pong.lua` (**–**) and click **Send ping**.
     - **Pass:** "pong not loaded", and the button's number stays the same.
  3. Add `pong.lua` back and click.
     - **Pass:** the ping goes through with that number.
- **Ping-pong: reloading ping while pong keeps answering** (2 min):
  - **Setup:** run `py tests/websocket/test_ping_pong.py --drive-pong`. It switches
    to "LuaBridge PingPong", sends pong a `ping` about 5 times a second (so pong
    keeps emitting `ponged`), and prints ping's `last_pong` every second.
  - **Meanwhile:** reload `ping.lua` from Tools → Scripts 5 times.
  - **Pass:**
    - no crash;
    - no Lua errors in the Script Log;
    - ping's `last_pong` resumes updating after each reload (the printout stops
      saying "not updating" within a second or two).

    `bridge.shutdown()` in `script_unload` disconnects the event handler, so a
    reloaded ping never leaves a stale listener behind.
  - Stop with Ctrl+C.
