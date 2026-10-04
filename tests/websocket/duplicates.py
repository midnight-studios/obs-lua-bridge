"""Duplicated script instances with instance IDs (M5).

Uses two scene collections made by setup_test_obs.py:
  * "LuaBridge Dup": stopwatch-demo and scoreboard copies with instance IDs
    a and b -> owners stopwatch.a, stopwatch.b, scoreboard.a, scoreboard.b.
    They must all register, work independently, and log no conflict warning.
  * "LuaBridge Conflict": two stopwatch copies with the same instance ID x ->
    the plugin must log "owner 'stopwatch.x' was already registered and active".
Ends back in the collection it started in.

Run:  py tests/websocket/duplicates.py
"""

import os
import pathlib
import sys
import time

import obsws_python as obsws

from test_vendor import call, connect_error, connect_kwargs

LOG_DIR = pathlib.Path(os.environ.get("OBS_LOG_DIR", "C:/obs-test/config/obs-studio/logs"))
CONFLICT = "owner 'stopwatch.x' was already registered and active"


def owners(client):
    return {o["owner"] for o in call(client, "ListOwners").get("owners", [])}


def wait_owners(client, wanted, timeout=15):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        current = owners(client)
        if wanted <= current:
            return current
        time.sleep(0.1)
    return owners(client)


def state(client, owner):
    return call(client, "GetState", {"owner": owner}).get("state", {})


def wait_state(client, owner, predicate, timeout=5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate(state(client, owner)):
            return True
        time.sleep(0.05)
    return False


def main():
    try:
        client = obsws.ReqClient(**{**connect_kwargs(), "timeout": 20})
    except Exception as exc:
        sys.exit(connect_error(exc))
    start = client.send("GetSceneCollectionList", raw=True)["currentSceneCollectionName"]
    log_path = sorted(LOG_DIR.glob("*.txt"), key=lambda p: p.stat().st_mtime)[-1]
    offset = log_path.stat().st_size
    problems = []

    client.send("SetCurrentSceneCollection", {"sceneCollectionName": "LuaBridge Dup"})
    dup = {"stopwatch.a", "stopwatch.b", "scoreboard.a", "scoreboard.b"}
    current = wait_owners(client, dup)
    if not dup <= current:
        problems.append(f"missing owners: {sorted(dup - current)}")
    if {"stopwatch", "scoreboard"} & current:
        problems.append("a copy registered under the base owner instead of its instance ID")
    names = {o["owner"]: o["display_name"] for o in call(client, "ListOwners")["owners"]}
    print("owners:", ", ".join(f"{o} ({names.get(o)})" for o in sorted(current)))

    # Independent: commands to one instance don't touch the other
    for o in ("stopwatch.a", "stopwatch.b"):
        call(client, "RunCommand", {"owner": o, "command": "reset"})
    wait_state(client, "stopwatch.b", lambda s: s.get("elapsed") == 0)
    call(client, "RunCommand", {"owner": "stopwatch.a", "command": "add", "data": {"seconds": 42}})
    if not wait_state(client, "stopwatch.a", lambda s: s.get("elapsed") == 42):
        problems.append("stopwatch.a didn't take the command")
    time.sleep(0.5)
    if state(client, "stopwatch.b").get("elapsed") != 0:
        problems.append("stopwatch.b changed when stopwatch.a got a command")
    before_b = state(client, "scoreboard.b").get("home", 0)
    before_a = state(client, "scoreboard.a").get("home", 0)
    call(client, "RunCommand", {"owner": "scoreboard.a", "command": "home_plus"})
    if not wait_state(client, "scoreboard.a", lambda s: s.get("home") == before_a + 1):
        problems.append("scoreboard.a didn't take the command")
    time.sleep(0.5)
    if state(client, "scoreboard.b").get("home", 0) != before_b:
        problems.append("scoreboard.b changed when scoreboard.a got a command")
    call(client, "RunCommand", {"owner": "scoreboard.a", "command": "home_minus"})
    call(client, "RunCommand", {"owner": "stopwatch.a", "command": "reset"})

    with open(log_path, "rb") as f:
        f.seek(offset)
        dup_log = f.read().decode("utf-8", errors="replace")
    if "already registered and active" in dup_log:
        problems.append("conflict warning logged although every copy has its own instance ID")
    offset = log_path.stat().st_size

    # Same instance ID twice: the safeguard warning must appear
    client.send("SetCurrentSceneCollection", {"sceneCollectionName": "LuaBridge Conflict"})
    wait_owners(client, {"stopwatch.x"})
    time.sleep(1)
    with open(log_path, "rb") as f:
        f.seek(offset)
        conflict_log = f.read().decode("utf-8", errors="replace")
    if CONFLICT not in conflict_log:
        problems.append("no conflict warning for two copies with the same instance ID")
    else:
        print("conflict warning logged as expected:", next(l for l in conflict_log.splitlines() if CONFLICT in l)
              .split("] ", 1)[-1])

    client.send("SetCurrentSceneCollection", {"sceneCollectionName": start})
    wait_owners(client, {"wstest"})
    client.disconnect()
    for p in problems:
        print("PROBLEM", p)
    print("RESULT:", "PASS" if not problems else f"FAIL ({len(problems)} problems)")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
