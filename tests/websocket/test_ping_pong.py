"""In-OBS tests for the ping-pong example (lua/examples/ping-pong/).

Uses the collections "LuaBridge PingPong" (ping.lua + pong.lua) and
"LuaBridge Ping Only" (ping.lua), made by setup_test_obs.py, and drives
ping's "send" command over websocket as if the dock button were clicked.

Default run (test OBS running; about 1 minute):
  1. ping without pong: "pong not loaded", the number isn't used up, and the
     plugin's "owner not registered" warning is deduplicated (at most 2 lines
     for 20 sends);
  2. round trip: 50 sends at 5/s -> pong counted 50, ping shows pong #50;
  3. rapid clicking: 300 sends as fast as possible. The websocket rate limit
     rejects the excess; every accepted send gives exactly one pong; GetInfo
     keeps answering within 1 s (the pass rule). p99 latency is reported only.
  Ends in the collection it started in.

  --close   4. closes the test OBS while both scripts are loaded and busy, and
            checks the shutdown like shutdown_in_flight.py does. Afterwards it
            restores the current collection in user.ini, so the next start
            opens the collection that was open before.
  --drive-pong [--minutes 5]
            For the manual ping-reload test (docs/TESTING.md): switches to
            "LuaBridge PingPong" and sends pong "ping" at about 5/s so pong
            keeps emitting "ponged", printing ping's last_pong every second.

Run:  py tests/websocket/test_ping_pong.py [--close | --drive-pong]
"""

import argparse
import statistics
import subprocess
import sys
import threading
import time

import obsws_python as obsws
from obsws_python.error import OBSSDKRequestError

from shutdown_in_flight import CLOSE, CONFIG, check_log, timeline
from test_vendor import call, connect_error, connect_kwargs

BOTH = "LuaBridge PingPong"
PING_ONLY = "LuaBridge Ping Only"
NOT_REGISTERED = "luabridge_run_command(pong): owner not registered"


def latest_log():
    return sorted((CONFIG / "logs").glob("*.txt"), key=lambda p: p.stat().st_mtime)[-1]


def log_count(text):
    return latest_log().read_text(encoding="utf-8", errors="replace").count(text)


def state(client, owner):
    return call(client, "GetState", {"owner": owner}).get("state", {})


def send(client, owner, command, data=None):
    request = {"owner": owner, "command": command}
    if data is not None:
        request["data"] = data
    return call(client, "RunCommand", request)


def switch(client, collection, present, absent=(), timeout=20):
    """Switches collections and waits until the owners in present are registered and those in absent aren't."""
    client.send("SetCurrentSceneCollection", {"sceneCollectionName": collection})
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            owners = {o["owner"] for o in call(client, "ListOwners").get("owners", [])}
            if set(present) <= owners and not (set(absent) & owners):
                time.sleep(0.5)  # let script_load finish setting state
                return
        except OBSSDKRequestError:
            pass  # "OBS is not ready" while the collection loads
        time.sleep(0.2)
    raise AssertionError(f"{collection}: owners {present} never registered (or {absent} never left)")


def wait_for(predicate, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return True
        time.sleep(0.05)
    return predicate()


def check_ping_without_pong(client, problems):
    switch(client, PING_ONLY, present=["ping"], absent=["pong"])
    before = log_count(NOT_REGISTERED)
    for _ in range(20):
        send(client, "ping", "send")
        time.sleep(0.1)
    time.sleep(0.5)
    s = state(client, "ping")
    lines = log_count(NOT_REGISTERED) - before
    print(f"1. ping without pong: result={s.get('result')!r}, button={s.get('next_label')!r}, "
          f"'owner not registered' log lines for 20 sends: {lines}")
    if s.get("result") != "pong not loaded":
        problems.append(f"1: result is {s.get('result')!r}, expected 'pong not loaded'")
    if s.get("next_label") != "Send ping #1":
        problems.append(f"1: button is {s.get('next_label')!r}; the number should not be used up")
    if not 1 <= lines <= 2:
        problems.append(f"1: {lines} 'owner not registered' log lines, expected 1-2 (deduplicated)")


def check_round_trip(client, problems):
    switch(client, BOTH, present=["ping", "pong"])
    start = state(client, "pong").get("count", 0)
    for _ in range(50):
        send(client, "ping", "send")
        time.sleep(0.2)
    done = wait_for(lambda: state(client, "pong").get("count") == start + 50
                    and "#50 " in state(client, "ping").get("last_pong", ""), timeout=2)
    s, p = state(client, "ping"), state(client, "pong")
    print(f"2. round trip: pong count {p.get('count')} (expected {start + 50}), ping: {s.get('result')!r}, "
          f"{s.get('last_pong')!r}")
    if not done:
        problems.append("2: pong count or ping's last_pong didn't reach #50 within 2 s")


def check_rapid(client, problems):
    start_count = state(client, "pong").get("count", 0)
    start_sent = int(state(client, "ping").get("result", "Ping #0 sent").split("#")[1].split()[0])
    stop = threading.Event()
    info_times = []

    def poll_info():
        poller = obsws.ReqClient(**{**connect_kwargs(), "timeout": 5})
        try:
            while not stop.is_set():
                t = time.perf_counter()
                call(poller, "GetInfo")
                info_times.append(time.perf_counter() - t)
                time.sleep(0.1)
        finally:
            poller.disconnect()

    poller = threading.Thread(target=poll_info)
    poller.start()
    accepted, limited, other, latencies = 0, 0, 0, []
    for _ in range(300):
        t = time.perf_counter()
        r = send(client, "ping", "send")
        latencies.append(time.perf_counter() - t)
        if r.get("ok"):
            accepted += 1
        elif "rate limited" in r.get("error", ""):
            limited += 1
        else:
            other += 1
    done = wait_for(lambda: state(client, "pong").get("count") == start_count + accepted, timeout=5)
    time.sleep(1)
    stop.set()
    poller.join()

    p = state(client, "pong")
    p99 = statistics.quantiles(latencies, n=100)[98] * 1000
    worst_info = max(info_times) if info_times else float("inf")
    print(f"3. rapid: 300 sends -> {accepted} accepted, {limited} rate limited, {other} other; "
          f"pong count +{p.get('count', 0) - start_count}; ping result {state(client, 'ping').get('result')!r}; "
          f"GetInfo worst {worst_info * 1000:.0f} ms over {len(info_times)} polls; "
          f"RunCommand p99 {p99:.1f} ms (reported only)")
    if other:
        problems.append(f"3: {other} sends failed with something other than the rate limit")
    if limited == 0:
        problems.append("3: nothing was rate limited; the test didn't reach the limit")
    if not done:
        problems.append(f"3: pong count {p.get('count')} != {start_count} + {accepted} accepted")
    if f"Ping #{start_sent + accepted} sent" != state(client, "ping").get("result"):
        problems.append("3: ping's last result doesn't match the number of accepted sends")
    if worst_info > 1.0:
        problems.append(f"3: GetInfo took {worst_info:.2f} s (limit 1 s)")


USER_INI = CONFIG / "user.ini"
COLLECTION_KEYS = ("SceneCollection=", "SceneCollectionFile=")


def saved_collection():
    """The current-collection lines of user.ini (OBS writes them when it exits)."""
    return [line for line in USER_INI.read_text(encoding="utf-8-sig").splitlines() if line.startswith(COLLECTION_KEYS)]


def restore_collection(lines):
    """Writes the saved current-collection lines back, so the next start opens that collection again."""
    keep = {line.split("=", 1)[0]: line for line in lines}
    text = USER_INI.read_text(encoding="utf-8-sig").splitlines()
    out = [keep.get(line.split("=", 1)[0], line) if line.startswith(COLLECTION_KEYS) else line for line in text]
    USER_INI.write_text("\n".join(out) + "\n", encoding="utf-8")


def check_close(problems):
    original = saved_collection()
    client = obsws.ReqClient(**connect_kwargs())
    switch(client, BOTH, present=["ping", "pong"])
    log_path = latest_log()
    client.disconnect()
    crashes = CONFIG / "crashes"
    crash_before = set(crashes.glob("*")) if crashes.exists() else set()

    stop, counter = threading.Event(), [0]

    def traffic(owner, command, data):
        c = None
        try:
            c = obsws.ReqClient(**{**connect_kwargs(), "timeout": 2})
            n = 0
            while not stop.is_set():
                n += 1
                send(c, owner, command, data(n) if data else None)
                counter[0] += 1
                time.sleep(0.05)
        except Exception:
            pass  # OBS stopped answering or closed the connection while exiting
        finally:
            if c is not None:
                try:
                    c.disconnect()  # an open, unread connection would hold up OBS's exit
                except Exception:
                    pass

    threads = [threading.Thread(target=traffic, args=("ping", "send", None)),
               threading.Thread(target=traffic, args=("pong", "ping", lambda n: {"n": n}))]
    for t in threads:
        t.start()
    time.sleep(2)
    started = time.monotonic()
    closed = subprocess.run(["pwsh", "-NoProfile", "-File", str(CLOSE), "-TimeoutSeconds", "15"],
                            capture_output=True, text=True)
    exit_s = time.monotonic() - started
    stop.set()
    for t in threads:
        t.join(timeout=10)

    if closed.returncode != 0:
        problems.append("4: OBS did not close within 15 s: " + closed.stdout.strip())
        return
    restore_collection(original)  # OBS saved "LuaBridge PingPong" as current when it exited
    problems += ["4: " + p for p in check_log(log_path)]
    new_crashes = (set(crashes.glob("*")) if crashes.exists() else set()) - crash_before
    if new_crashes:
        problems.append(f"4: crash dump(s): {[p.name for p in new_crashes]}")
    print(f"4. close while busy: {counter[0]} requests in flight window, closed in {exit_s:.1f} s; "
          f"{timeline(log_path)}")


def drive_pong(minutes):
    client = obsws.ReqClient(**connect_kwargs())
    original = client.send("GetSceneCollectionList", raw=True)["currentSceneCollectionName"]
    switch(client, BOTH, present=["ping", "pong"])
    print("Driving pong at ~5/s. Now reload ping.lua in Tools > Scripts 5 times. Ctrl+C to stop.")
    deadline, n, last = time.monotonic() + minutes * 60, 0, None
    try:
        while time.monotonic() < deadline:
            n += 1
            try:
                send(client, "pong", "ping", {"n": n})
                if n % 5 == 0:
                    shown = state(client, "ping").get("last_pong", "(ping not registered)")
                    print(f"  sent #{n}; ping shows: {shown}" + ("" if shown != last else "   <- not updating"))
                    last = shown
            except OBSSDKRequestError as exc:
                print(f"  sent #{n}: {exc}")
            time.sleep(0.2)
    except KeyboardInterrupt:
        pass
    client.send("SetCurrentSceneCollection", {"sceneCollectionName": original})
    client.disconnect()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--close", action="store_true", help="check 4: close OBS while both scripts are busy")
    parser.add_argument("--drive-pong", action="store_true", help="send pong pings for the manual reload test")
    parser.add_argument("--minutes", type=float, default=5)
    args = parser.parse_args()

    if args.drive_pong:
        drive_pong(args.minutes)
        return 0

    problems = []
    if args.close:
        check_close(problems)
    else:
        try:
            client = obsws.ReqClient(**connect_kwargs())
        except Exception as exc:
            sys.exit(connect_error(exc))
        original = client.send("GetSceneCollectionList", raw=True)["currentSceneCollectionName"]
        try:
            check_ping_without_pong(client, problems)
            check_round_trip(client, problems)
            check_rapid(client, problems)
        finally:
            client.send("SetCurrentSceneCollection", {"sceneCollectionName": original})
            client.disconnect()

    for p in problems:
        print("PROBLEM", p)
    print("RESULT:", "PASS" if not problems else f"FAIL ({len(problems)} problems)")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
