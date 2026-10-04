"""Prepares the portable test OBS for the M5 tests. Run with OBS closed.

Backs up <config>/basic, then (idempotently):
  * adds tests/lua/stress-owners.lua to the current collection ("Untitled");
  * creates the collection "LuaBridge B" (ws-companion, hello-bridge,
    scoreboard, stress-owners) for switching.py;
  * copies the examples to <obs>/scripts-dup/examples/ (with luabridge.lua one
    level up, as a user would install it) and creates the collections
    "LuaBridge Dup" (instance IDs a and b) and "LuaBridge Conflict" (two copies
    with instance ID x) for duplicates.py;
  * creates "LuaBridge Fuzz" (fuzz-harness.lua), and "LuaBridge PingPong" /
    "LuaBridge Ping Only" (the ping-pong example) for test_ping_pong.py;
  * creates the profile "LuaBridge P2" as a copy of "Untitled".
Only C:/obs-test (or OBS_ROOT) is touched.

Run:  py tests/websocket/setup_test_obs.py --backup <dir>
"""

import argparse
import copy
import datetime
import json
import os
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OBS_ROOT = pathlib.Path(os.environ.get("OBS_ROOT", r"C:\obs-test"))
BASIC = OBS_ROOT / "config" / "obs-studio" / "basic"
DUP = OBS_ROOT / "scripts-dup"


def posix(p):
    return pathlib.Path(p).as_posix()


def script(path, **settings):
    return {"path": posix(path), "settings": settings}


def write_collection(base, name, filename, scripts):
    data = copy.deepcopy(base)
    data["name"] = name
    data.setdefault("modules", {})["scripts-tool"] = scripts
    (BASIC / "scenes" / filename).write_text(json.dumps(data, indent=4), encoding="utf-8")
    print(f"collection '{name}': {len(scripts)} scripts")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--backup", required=True, help="directory for a backup of the test OBS 'basic' config")
    args = parser.parse_args()

    running = subprocess.run(["tasklist", "/FI", "IMAGENAME eq obs64.exe", "/NH"], capture_output=True, text=True)
    if "obs64.exe" in running.stdout:
        sys.exit("close the test OBS first (OBS rewrites these files when it closes)")

    backup = pathlib.Path(args.backup) / f"basic-{datetime.datetime.now():%Y%m%d-%H%M%S}"
    shutil.copytree(BASIC, backup)
    print(f"backup: {backup}")

    main_file = BASIC / "scenes" / "Untitled.json"
    base = json.loads(main_file.read_text(encoding="utf-8"))
    scripts = base.setdefault("modules", {}).setdefault("scripts-tool", [])
    stress = posix(ROOT / "tests/lua/stress-owners.lua")
    if not any(s["path"] == stress for s in scripts):
        scripts.append(script(stress))
        main_file.write_text(json.dumps(base, indent=4), encoding="utf-8")
        print("added stress-owners.lua to 'Untitled'")

    write_collection(base, "LuaBridge B", "LuaBridge_B.json", [
        script(ROOT / "tests/websocket/ws-companion.lua"),
        script(ROOT / "lua/examples/hello-bridge.lua"),
        script(ROOT / "lua/examples/scoreboard.lua"),
        script(stress),
    ])

    examples = DUP / "examples"
    examples.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / "lua/luabridge.lua", DUP / "luabridge.lua")
    copies = {
        "stopwatch-a.lua": "stopwatch-demo.lua",
        "stopwatch-b.lua": "stopwatch-demo.lua",
        "stopwatch-x1.lua": "stopwatch-demo.lua",
        "stopwatch-x2.lua": "stopwatch-demo.lua",
        "scoreboard-a.lua": "scoreboard.lua",
        "scoreboard-b.lua": "scoreboard.lua",
    }
    for target, source in copies.items():
        shutil.copy2(ROOT / "lua/examples" / source, examples / target)
    write_collection(base, "LuaBridge Dup", "LuaBridge_Dup.json", [
        script(examples / "stopwatch-a.lua", instance_id="a"),
        script(examples / "stopwatch-b.lua", instance_id="b"),
        script(examples / "scoreboard-a.lua", instance_id="a"),
        script(examples / "scoreboard-b.lua", instance_id="b"),
    ])
    write_collection(base, "LuaBridge Conflict", "LuaBridge_Conflict.json", [
        script(examples / "stopwatch-x1.lua", instance_id="x"),
        script(examples / "stopwatch-x2.lua", instance_id="x"),
    ])

    # Switching to this collection runs the procedure fuzzer once
    write_collection(base, "LuaBridge Fuzz", "LuaBridge_Fuzz.json", [script(ROOT / "tests/lua/fuzz-harness.lua")])

    # Ping-pong example (test_ping_pong.py): both halves, and ping without pong
    ping_pong = ROOT / "lua/examples/ping-pong"
    write_collection(base, "LuaBridge PingPong", "LuaBridge_PingPong.json", [
        script(ROOT / "tests/websocket/ws-companion.lua"),
        script(ping_pong / "ping.lua"),
        script(ping_pong / "pong.lua"),
    ])
    write_collection(base, "LuaBridge Ping Only", "LuaBridge_Ping_Only.json", [
        script(ROOT / "tests/websocket/ws-companion.lua"),
        script(ping_pong / "ping.lua"),
    ])

    profile = BASIC / "profiles" / "LuaBridge_P2"
    if not profile.exists():
        shutil.copytree(BASIC / "profiles" / "Untitled", profile)
        ini = profile / "basic.ini"
        text = ini.read_text(encoding="utf-8-sig")
        lines = ["Name=LuaBridge P2" if line.startswith("Name=") else line for line in text.splitlines()]
        ini.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("profile 'LuaBridge P2' created")
    print("done")


if __name__ == "__main__":
    main()
