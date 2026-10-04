"""Closes OBS while commands, events and dock updates are in flight (M5).

Each cycle starts the test OBS, waits for the stress owners, starts websocket
traffic (RunCommand hits -> StateChanged / CustomEvent / dock updates, plus
Stopwatch toggles), then closes OBS normally (tools/close-test-obs.ps1) in the
middle of it. A cycle passes when OBS exits within 15 s, writes no crash dump,
logs nothing from [lua-bridge] after "==== Shutting down" except
"plugin unloaded", and ends its log with "Number of memory leaks: 0".

Prerequisites: test OBS NOT running; tests/lua/stress-owners.lua and
lua/examples/stopwatch-demo.lua in its scene collection; OBS_WS_* set.
Run:  py tests/websocket/shutdown_in_flight.py [--cycles 10]
"""

import argparse
import os
import pathlib
import re
import subprocess
import sys
import threading
import time

import obsws_python as obsws

from test_vendor import call, connect_kwargs

OBS_EXE = pathlib.Path(os.environ.get("OBS_EXE", r"C:\obs-test\bin\64bit\obs64.exe"))
CONFIG = pathlib.Path(os.environ.get("OBS_CONFIG", r"C:\obs-test\config\obs-studio"))
ROOT = pathlib.Path(__file__).resolve().parents[2]
CLOSE = ROOT / "tools" / "close-test-obs.ps1"


def obs_running():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq obs64.exe", "/FO", "CSV", "/NH"],
                         capture_output=True, text=True).stdout
    return "obs64.exe" in out


def wait_for_owners(timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        client = None
        try:
            client = obsws.ReqClient(**connect_kwargs())
            owners = {o["owner"] for o in call(client, "ListOwners").get("owners", [])}
            if {f"stress.{i}" for i in range(10)} <= owners:
                return client
        except Exception:
            pass  # OBS still starting (no server or no vendor yet)
        # Never leave a connection open: an unread one holds up OBS's exit
        if client is not None:
            try:
                client.disconnect()
            except Exception:
                pass
        time.sleep(0.5)
    return None


def traffic(stop, counter):
    """Sends commands until stopped or the connection drops (expected at exit).

    Behaves like a well-behaved real client: once OBS stops answering (a request
    times out after 2 s) or closes the connection, it disconnects. A client that
    keeps its socket open without reading it holds up obs-websocket's unload,
    which would measure the test client instead of Lua Bridge.
    """
    client = None
    try:
        client = obsws.ReqClient(**{**connect_kwargs(), "timeout": 2})
        n = 0
        while not stop.is_set():
            owner = f"stress.{n % 10}"
            call(client, "RunCommand", {"owner": owner, "command": "hit", "data": {"n": n}})
            if n % 25 == 0:
                call(client, "RunCommand", {"owner": "stopwatch", "command": "toggle"})
            n += 1
            counter[0] += 1
            time.sleep(0.004)  # ~150-250 requests/s across threads: near the global limit
    except Exception:
        pass  # OBS stopped answering or closed the connection while exiting
    finally:
        if client is not None:
            try:
                client.disconnect()
            except Exception:
                pass


def seconds(line):
    """Seconds since midnight of a log line's hh:mm:ss.mmm timestamp, or None."""
    match = re.match(r"(\d\d):(\d\d):(\d\d\.\d+)", line)
    return int(match[1]) * 3600 + int(match[2]) * 60 + float(match[3]) if match else None


def timeline(log_path):
    """How long obs-websocket's unload and the whole shutdown (to our unload) took."""
    marks = {}
    patterns = {
        "shutdown": "==== Shutting down",
        "ws_start": "[obs_module_unload] Shutting down",
        "ws_end": "[obs_module_unload] Finished",
        "unloaded": "[lua-bridge] plugin unloaded",
    }
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        for key, pattern in patterns.items():
            if pattern in line and seconds(line) is not None:
                marks[key] = seconds(line)
    if marks.keys() != patterns.keys():
        return "timeline incomplete"
    return (f"shutdown -> plugin unloaded {marks['unloaded'] - marks['shutdown']:.1f} s, "
            f"of which obs-websocket unload {marks['ws_end'] - marks['ws_start']:.1f} s")


def check_log(log_path):
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    try:
        shutdown = next(i for i, line in enumerate(lines) if "==== Shutting down" in line)
    except StopIteration:
        return ["no '==== Shutting down' in the log"]
    problems = []
    for line in lines[shutdown:]:
        if "[lua-bridge]" in line and "plugin unloaded" not in line:
            problems.append("after shutdown: " + line.strip())
        if any(s in line for s in ("could not send", "unexpected exception", "request failed")):
            problems.append(line.strip())
    if not lines or "Number of memory leaks: 0" not in lines[-1]:
        problems.append("last line: " + (lines[-1].strip() if lines else "(empty log)"))
    return problems


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cycles", type=int, default=10)
    parser.add_argument("--traffic-seconds", type=float, default=3)
    args = parser.parse_args()
    if obs_running():
        sys.exit("close OBS first (this test starts and closes it)")

    crashes = CONFIG / "crashes"
    failures = 0
    for cycle in range(1, args.cycles + 1):
        crash_before = set(crashes.glob("*")) if crashes.exists() else set()
        subprocess.Popen([str(OBS_EXE)], cwd=str(OBS_EXE.parent))
        client = wait_for_owners()
        if not client:
            print(f"cycle {cycle}: FAIL (stress owners never registered)")
            return 1
        client.disconnect()
        log_path = sorted((CONFIG / "logs").glob("*.txt"), key=lambda p: p.stat().st_mtime)[-1]

        stop, counter = threading.Event(), [0]
        threads = [threading.Thread(target=traffic, args=(stop, counter)) for _ in range(3)]
        for t in threads:
            t.start()
        time.sleep(args.traffic_seconds)
        started = time.monotonic()
        closed = subprocess.run(["pwsh", "-NoProfile", "-File", str(CLOSE), "-TimeoutSeconds", "15"],
                                capture_output=True, text=True)
        exit_s = time.monotonic() - started
        stop.set()
        for t in threads:
            t.join(timeout=10)

        problems = [] if closed.returncode == 0 else ["did not close: " + closed.stdout.strip()]
        new_crashes = (set(crashes.glob("*")) if crashes.exists() else set()) - crash_before
        if new_crashes:
            problems.append(f"crash dump(s): {[p.name for p in new_crashes]}")
        if closed.returncode == 0:
            problems += check_log(log_path)
        status = "PASS" if not problems else "FAIL"
        print(f"cycle {cycle}: {status} ({counter[0]} requests in flight window, closed in {exit_s:.1f} s; "
              f"{timeline(log_path)}; log {log_path.name})", flush=True)
        for p in problems[:8]:
            print("   ", p)
        if problems:
            failures += 1
            if closed.returncode != 0:
                return 1  # OBS still open: stop instead of starting another instance
    print(f"{args.cycles - failures}/{args.cycles} cycles passed")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
