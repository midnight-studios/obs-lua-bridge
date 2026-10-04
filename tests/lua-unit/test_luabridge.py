"""Runs the Lua unit tests for lua/luabridge.lua and the example scripts.

Tests are in test_luabridge.lua (the helper) and test_examples.lua (the
examples never log a warning in normal use, with or without the plugin).

Uses lupa's LuaJIT 2.1 runtime, the same Lua that OBS embeds, so Lua 5.1
compatibility is tested for real. Install with:  py -m pip install lupa

Run:  py tests/lua-unit/test_luabridge.py
Each test runs in a fresh Lua state with a fake obslua (fake_obslua.lua).
"""

import pathlib
import sys

try:
    from lupa import luajit21  # LuaJIT 2.1 only; never fall back to another Lua
except ImportError as exc:  # pragma: no cover
    sys.exit(f"lupa with its LuaJIT 2.1 runtime is required (py -m pip install lupa): {exc}")

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
HELPER = (ROOT / "lua" / "luabridge.lua").as_posix()
EXAMPLES = (ROOT / "lua" / "examples").as_posix() + "/"
FAKE = (HERE / "fake_obslua.lua").as_posix()
TEST_FILES = [(HERE / name).as_posix() for name in ("test_luabridge.lua", "test_examples.lua")]


def new_runtime():
    lua = luajit21.LuaRuntime(unpack_returned_tuples=True)
    version = lua.eval("_VERSION")
    jit = lua.eval("jit and jit.version")
    if version != "Lua 5.1" or not str(jit).startswith("LuaJIT 2.1"):
        sys.exit(f"expected LuaJIT 2.1 (Lua 5.1), got {version} / {jit}")
    return lua


def main():
    probe = new_runtime()
    print(f"runtime: {probe.eval('jit.version')} ({probe.eval('_VERSION')})")

    failed = total = 0
    for tests_file in TEST_FILES:
        names = sorted(probe.execute(f"return dofile('{tests_file}')").keys())
        for name in names:
            total += 1
            lua = new_runtime()
            run = lua.eval(
                f"""
                function(name)
                    local fake = dofile('{FAKE}')
                    local tests = dofile('{tests_file}')
                    local function load_helper() return dofile('{HELPER}') end
                    -- Loads an example as OBS does: script_path() is its folder
                    local function load_example(file)
                        script_properties, script_update = nil, nil
                        script_path = function() return '{EXAMPLES}' end
                        dofile('{EXAMPLES}' .. file)
                    end
                    local ok, err = pcall(tests[name], fake, load_helper, load_example)
                    return ok, err and tostring(err) or nil
                end
                """
            )
            ok, err = run(name)
            if ok:
                print(f"PASS {name}")
            else:
                failed += 1
                print(f"FAIL {name}: {err}")

    print(f"{total - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
