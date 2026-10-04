"""Stress / soak test for the LuaBridge vendor (M5).

Sends RunCommand "hit" to the ten owners of tests/lua/stress-owners.lua at a
steady rate, with the Stopwatch demo running throughout, and samples the OBS
process (private memory, working set, handles) to a CSV. At the end it checks:

  * no lost commands: each owner's "count" grew by exactly the number of
    commands it accepted;
  * memory plateau: private memory grew less than 5 % from the start of the
    measurement window to the end;
  * stable handles: within +-10 % of the window start;
  * p99 request latency below 50 ms;
  * no plugin errors (could not send / unexpected exception / request failed)
    in the OBS log.

Defaults are the M5 stress test (1,000 commands/min for 60 min, sampled every
30 s, window from minute 10). The overnight soak uses e.g.
  py tests/websocket/stress.py --minutes 480 --rate 6 --sample 300 --window-start 60

Prerequisites: test OBS running with tests/lua/stress-owners.lua and
lua/examples/stopwatch-demo.lua loaded; OBS_WS_* as for test_vendor.py.
"""

import argparse
import csv
import datetime
import os
import pathlib
import statistics
import sys
import time

import obsws_python as obsws
import psutil

from test_vendor import call, connect_error, connect_kwargs

OBS_EXE = os.environ.get("OBS_EXE", r"C:\obs-test\bin\64bit\obs64.exe")
LOG_DIR = pathlib.Path(os.environ.get("OBS_LOG_DIR", "C:/obs-test/config/obs-studio/logs"))
OWNERS = [f"stress.{i}" for i in range(10)]
ERRORS = ("could not send", "unexpected exception", "request failed", "rate limited")


def obs_process():
    for p in psutil.process_iter(["name", "exe"]):
        if (p.info["exe"] or "").lower() == OBS_EXE.lower():
            return p
    sys.exit(f"test OBS ({OBS_EXE}) is not running")


def sample(process, started):
    mem = process.memory_info()
    return {
        "time": datetime.datetime.now().isoformat(timespec="seconds"),
        "minute": round((time.monotonic() - started) / 60, 2),
        "private_mb": round(getattr(mem, "private", mem.rss) / 2**20, 1),
        "working_set_mb": round(mem.rss / 2**20, 1),
        "handles": process.num_handles() if hasattr(process, "num_handles") else 0,
    }


def counts(client):
    result = {}
    for owner in OWNERS:
        response = call(client, "GetState", {"owner": owner})
        if not response.get("ok"):
            sys.exit(f"{owner} is not registered: load tests/lua/stress-owners.lua ({response.get('error')})")
        result[owner] = response["state"].get("count", 0)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--minutes", type=float, default=60)
    parser.add_argument("--rate", type=float, default=1000, help="RunCommands per minute (all owners together)")
    parser.add_argument("--sample", type=float, default=30, help="seconds between process samples")
    parser.add_argument("--window-start", type=float, default=10, help="minute from which memory must plateau")
    parser.add_argument("--csv", default=None, help="CSV path (default: stress-<timestamp>.csv here)")
    args = parser.parse_args()

    process = obs_process()
    try:
        client = obsws.ReqClient(**connect_kwargs())
    except Exception as exc:
        sys.exit(connect_error(exc))

    if call(client, "GetState", {"owner": "stopwatch"}).get("ok"):
        if not call(client, "GetState", {"owner": "stopwatch"})["state"].get("running"):
            call(client, "RunCommand", {"owner": "stopwatch", "command": "start"})
        print("stopwatch demo: running")
    else:
        print("note: stopwatch demo not loaded")

    log_path = sorted(LOG_DIR.glob("*.txt"), key=lambda p: p.stat().st_mtime)[-1]
    log_offset = log_path.stat().st_size
    start_counts = counts(client)
    csv_path = pathlib.Path(args.csv or f"stress-{datetime.datetime.now():%Y%m%d-%H%M%S}.csv")
    rows, latencies = [], []
    sent = {o: 0 for o in OWNERS}
    accepted = {o: 0 for o in OWNERS}
    rejected = []
    interval = 60.0 / args.rate
    started = time.monotonic()
    end_at = started + args.minutes * 60
    next_send = started
    next_sample = started
    next_state = started + 60
    n = 0

    print(f"running {args.minutes:g} min at {args.rate:g} commands/min; samples every {args.sample:g} s -> {csv_path}")
    while True:
        now = time.monotonic()
        if now >= next_sample:
            row = sample(process, started)
            rows.append(row)
            print(f"  min {row['minute']:6.1f}: private {row['private_mb']} MB, working set "
                  f"{row['working_set_mb']} MB, handles {row['handles']}, sent {n}, rejected {len(rejected)}",
                  flush=True)
            next_sample += args.sample
        if now >= end_at:
            break
        if now >= next_state:
            counts(client)  # one GetState per owner per minute
            next_state += 60
        if now >= next_send:
            owner = OWNERS[n % len(OWNERS)]
            t0 = time.perf_counter()
            response = call(client, "RunCommand", {"owner": owner, "command": "hit", "data": {"n": n}})
            latencies.append((time.perf_counter() - t0) * 1000)
            sent[owner] += 1
            if response.get("accepted"):
                accepted[owner] += 1
            else:
                rejected.append(response.get("error"))
            n += 1
            next_send += interval
        else:
            time.sleep(min(next_send, next_sample, next_state, end_at) - now)

    time.sleep(3)  # let the last commands reach the script
    end_counts = counts(client)
    client.disconnect()
    rows.append(sample(process, started))

    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    window = [r for r in rows if r["minute"] >= min(args.window_start, args.minutes / 6)]
    first, last = window[0], window[-1]
    growth = (last["private_mb"] - first["private_mb"]) / first["private_mb"] * 100
    handle_change = (last["handles"] - first["handles"]) / max(first["handles"], 1) * 100
    lost = {o: accepted[o] - (end_counts[o] - start_counts[o]) for o in OWNERS}
    p50 = statistics.median(latencies) if latencies else 0
    p99 = statistics.quantiles(latencies, n=100)[98] if len(latencies) >= 100 else max(latencies, default=0)
    with open(log_path, "rb") as f:
        f.seek(log_offset)
        new_log = f.read().decode("utf-8", errors="replace").splitlines()
    log_errors = [line for line in new_log if "[lua-bridge]" in line and any(e in line for e in ERRORS)]

    checks = {
        "no lost commands": all(v == 0 for v in lost.values()),
        "all commands accepted": not rejected,
        f"memory plateau (<5 % from min {first['minute']:g}: {growth:+.1f} %)": growth < 5,
        f"handles stable ({first['handles']} -> {last['handles']}, {handle_change:+.1f} %)": abs(handle_change) <= 10,
        f"p99 latency < 50 ms (p50 {p50:.1f} ms, p99 {p99:.1f} ms)": p99 < 50,
        f"no plugin errors in the log ({len(log_errors)})": not log_errors,
    }
    print(f"\n{n} commands sent in {(time.monotonic() - started) / 60:.1f} min; "
          f"memory {rows[0]['private_mb']} -> {last['private_mb']} MB private")
    for name, ok in checks.items():
        print(f"{'PASS' if ok else 'FAIL'}: {name}")
    if any(lost.values()):
        print("lost per owner:", lost)
    for line in log_errors[:10]:
        print("LOG", line)
    print(f"CSV: {csv_path.resolve()}")
    return 0 if all(checks.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
