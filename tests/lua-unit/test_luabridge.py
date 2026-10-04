"""Runs the Lua unit tests for lua/luabridge.lua (tests in test_luabridge.lua).

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
FAKE = (HERE / "fake_obslua.lua").as_posix()
TESTS = (HERE / "test_luabridge.lua").as_posix()


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
    names = sorted(probe.execute(f"return dofile('{TESTS}')").keys())

    failed = 0
    for name in names:
        lua = new_runtime()
        ok, err = lua.execute(
            f"""
            local fake = dofile('{FAKE}')
            local tests = dofile('{TESTS}')
            local function load_helper() return dofile('{HELPER}') end
            local ok, err = pcall(tests['{name}'], fake, load_helper)
            return ok, err and tostring(err) or nil
            """
        )
        if ok:
            print(f"PASS {name}")
        else:
            failed += 1
            print(f"FAIL {name}: {err}")

    print(f"{len(names) - failed} passed, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
