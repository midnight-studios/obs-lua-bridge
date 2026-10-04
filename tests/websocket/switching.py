"""Scene collection, profile and Studio Mode switching (M5).

OBS keeps scripts per scene collection, so switching collections unloads every
script of the old one and loads every script of the new one. This test switches
between two collections (default "Untitled" and "LuaBridge B") and between two
profiles, and after every switch checks:

  * ListOwners is exactly the set first seen for that collection (no leftover
    owners from the previous collection, none missing);
  * a RunCommand round trip to wstest (in both collections) works;
  * the OBS log gains no "already registered and active" warnings (that would
    mean new scripts registered before old ones unregistered) and no errors.

Then it repeats the collection switches while traffic flows to the stress
owners, toggles Studio Mode, and compares OBS memory before and after.

Prerequisites: both collections and profiles exist; ws-companion.lua is in
both collections. Run:  py tests/websocket/switching.py [--switches 50] [--profile-switches 20]
"""

import argparse
import os
import pathlib
import sys
import threading
import time

import obsws_python as obsws
import psutil

from test_vendor import call, connect_error, connect_kwargs

OBS_EXE = os.environ.get("OBS_EXE", r"C:\obs-test\bin\64bit\obs64.exe")
LOG_DIR = pathlib.Path(os.environ.get("OBS_LOG_DIR", "C:/obs-test/config/obs-studio/logs"))
BAD = ("already registered and active", "could not send", "unexpected exception", "request failed")


def private_mb():
    for p in psutil.process_iter(["exe"]):
        if (p.info["exe"] or "").lower() == OBS_EXE.lower():
            mem = p.memory_info()
            return round(getattr(mem, "private", mem.rss) / 2**20, 1)
    return None


def owners(client):
    return frozenset(o["owner"] for o in call(client, "ListOwners").get("owners", []))


def settled_owners(client, timeout=15):
    """Waits until ListOwners stops changing (scripts finished loading)."""
    deadline = time.monotonic() + timeout
    last, stable_since = None, time.monotonic()
    while time.monotonic() < deadline:
        current = owners(client)
        if current != last:
            last, stable_since = current, time.monotonic()
        elif time.monotonic() - stable_since > 0.6 and "wstest" in current:
            return current
        time.sleep(0.1)
    return last


def round_trip(client):
    before = call(client, "GetState", {"owner": "wstest"}).get("state", {}).get("ticks", 0)
    if not call(client, "RunCommand", {"owner": "wstest", "command": "tick"}).get("accepted"):
        return False
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        if call(client, "GetState", {"owner": "wstest"}).get("state", {}).get("ticks", 0) == before + 1:
            return True
        time.sleep(0.05)
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--collections", nargs=2, default=["Untitled", "LuaBridge B"])
    parser.add_argument("--profiles", nargs=2, default=["Untitled", "LuaBridge P2"])
    parser.add_argument("--switches", type=int, default=50)
    parser.add_argument("--profile-switches", type=int, default=20)
    args = parser.parse_args()

    try:
        client = obsws.ReqClient(**{**connect_kwargs(), "timeout": 20})
    except Exception as exc:
        sys.exit(connect_error(exc))
    log_path = sorted(LOG_DIR.glob("*.txt"), key=lambda p: p.stat().st_mtime)[-1]
    log_offset = log_path.stat().st_size
    problems = []
    expected = {}
    mem_before = private_mb()

    def switch_collection(name, label):
        client.send("SetCurrentSceneCollection", {"sceneCollectionName": name})
        current = settled_owners(client)
        if name not in expected:
            expected[name] = current
            print(f"  {name}: {len(current)} owners: {', '.join(sorted(current))}")
        elif current != expected[name]:
            problems.append(f"{label} -> {name}: owners {sorted(current ^ expected[name])} differ")
        if not round_trip(client):
            problems.append(f"{label} -> {name}: round trip failed")

    current = client.send("GetSceneCollectionList", raw=True)["currentSceneCollectionName"]
    print(f"start in collection '{current}', {mem_before} MB private")
    for i in range(args.switches):
        switch_collection(args.collections[i % 2 == 0], f"switch {i + 1}")
    if expected.get(args.collections[0]) == expected.get(args.collections[1]):
        problems.append("both collections have the same owners; the test can't tell them apart")
    print(f"{args.switches} collection switches done, {len(problems)} problems")

    # Profiles don't reload scripts: owners stay the same
    base = owners(client)
    for i in range(args.profile_switches):
        client.send("SetCurrentProfile", {"profileName": args.profiles[i % 2 == 0]})
        time.sleep(0.5)
        if owners(client) != base or not round_trip(client):
            problems.append(f"profile switch {i + 1}: owners changed or round trip failed")
    client.send("SetCurrentProfile", {"profileName": args.profiles[0]})
    print(f"{args.profile_switches} profile switches done, {len(problems)} problems")

    # Studio Mode on and off
    for enabled in (True, False):
        client.send("SetStudioModeEnabled", {"studioModeEnabled": enabled})
        time.sleep(0.5)
        if not round_trip(client):
            problems.append(f"Studio Mode {'on' if enabled else 'off'}: round trip failed")
    print("Studio Mode toggled on and off")

    # Collection switches again, with traffic to the stress owners
    stop, sent = threading.Event(), [0]

    def traffic():
        c = obsws.ReqClient(**connect_kwargs())
        n = 0
        while not stop.is_set():
            # owner not registered is expected while a collection is switching
            call(c, "RunCommand", {"owner": f"stress.{n % 10}", "command": "hit", "data": {"n": n}})
            n += 1
            sent[0] += 1
            time.sleep(0.05)
        c.disconnect()

    t = threading.Thread(target=traffic)
    t.start()
    for i in range(10):
        switch_collection(args.collections[i % 2 == 0], f"switch under load {i + 1}")
    stop.set()
    t.join()
    switch_collection(args.collections[0], "final")
    print(f"10 switches under load done ({sent[0]} commands sent), {len(problems)} problems")

    with open(log_path, "rb") as f:
        f.seek(log_offset)
        new_log = f.read().decode("utf-8", errors="replace").splitlines()
    bad = [line for line in new_log if "[lua-bridge]" in line and any(b in line for b in BAD)]
    problems += [f"log: {line.strip()}" for line in bad[:10]]
    client.disconnect()
    mem_after = private_mb()
    print(f"memory: {mem_before} -> {mem_after} MB private")
    for p in problems[:30]:
        print("PROBLEM", p)
    print("RESULT:", "PASS" if not problems else f"FAIL ({len(problems)} problems)")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
