# Testing Lua Bridge for OBS

Every test suite, how to run it, how long it takes, and what needs a person.
CI only builds and checks formatting; everything below runs locally.

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
