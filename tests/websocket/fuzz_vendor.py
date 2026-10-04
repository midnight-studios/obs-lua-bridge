"""Fuzzes the LuaBridge obs-websocket vendor requests (M5).

Sends generated requests to all five vendor requests: wrong types for every
field, missing fields, empty / 64 KiB +- 1 / 1 MB strings, deep nesting and
unusual Unicode. Every response must be a well-formed {ok:false, error} or a
valid success, and OBS must still answer GetInfo afterwards. Deterministic
(fixed seed); prints a summary per request.

Prerequisites: as test_vendor.py (ws-companion.lua loaded).
Run:  py tests/websocket/fuzz_vendor.py [--count 5000] [--seed 20261004]
"""

import argparse
import json
import random
import sys
import time
from collections import Counter

import obsws_python as obsws
from obsws_python.error import OBSSDKRequestError

from test_vendor import connect_error, connect_kwargs

VENDOR = "LuaBridge"
REQUESTS = ["GetInfo", "ListOwners", "ListCommands", "GetState", "RunCommand"]
OWNERS = ["wstest", "scoreboard", "stopwatch", "nobody", "Bad Owner", "", "a" * 64, "a" * 65, "x.y-z_1"]
COMMANDS = ["tick", "set", "echo", "emit", "nope", "", "Bad", "c" * 65]
UNICODE = [
    "café",
    "\U0001f3ae\U0001f468‍\U0001f469‍\U0001f467",  # emoji incl. ZWJ sequence
    "é́́",  # combining marks
    "‮RTL‬",  # bidi override
    "﻿BOM",
    "￿￾",
    "\u0000nul",
    "  ",
]


def random_scalar(rng):
    return rng.choice(
        [
            None,
            True,
            False,
            0,
            -1,
            2**63,
            -(2**63) - 1,
            1.5,
            1e308,
            -0.0,
            "",
            "x",
            rng.choice(UNICODE),
            "s" * rng.choice([1, 64, 65, 1000]),
        ]
    )


def random_value(rng, depth=0):
    kind = rng.randrange(6 if depth < 4 else 1)
    if kind == 0:
        return random_scalar(rng)
    if kind == 1:
        return [random_value(rng, depth + 1) for _ in range(rng.randrange(4))]
    if kind == 2:
        return {rng.choice(["a", "seconds", "value", "key", rng.choice(UNICODE)]): random_value(rng, depth + 1)
                for _ in range(rng.randrange(4))}
    if kind == 3:
        return rng.choice(UNICODE)
    if kind == 4:
        nested = {}
        for _ in range(rng.choice([10, 100, 500])):
            nested = {"n": nested}
        return nested
    return "z" * rng.choice([65535, 65536, 65537, 1_000_000])


def random_field(rng, good_values):
    roll = rng.random()
    if roll < 0.5:
        return rng.choice(good_values)
    if roll < 0.6:
        return rng.choice(UNICODE)
    return random_value(rng)


def random_request(rng, request_type):
    data = {}
    for field, good in (("owner", OWNERS), ("command", COMMANDS)):
        if rng.random() < 0.85:
            data[field] = random_field(rng, good)
    if rng.random() < 0.7:
        data["data"] = random_value(rng)
    if rng.random() < 0.1:
        data = random_value(rng)  # not even an object
    return data


def well_formed(response):
    if not isinstance(response, dict) or not isinstance(response.get("ok"), bool):
        return False
    if response["ok"] is False:
        return isinstance(response.get("error"), str) and response["error"] != ""
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--count", type=int, default=5000, help="requests per request type")
    parser.add_argument("--seed", type=int, default=20261004)
    parser.add_argument("--requests", nargs="+", default=REQUESTS, choices=REQUESTS,
                        help="request types to fuzz (default: all)")
    parser.add_argument("--vendor", default=VENDOR, help="vendor name (another name sends the same traffic "
                        "without reaching the plugin, as a control)")
    args = parser.parse_args()
    rng = random.Random(args.seed)

    try:
        client = obsws.ReqClient(**connect_kwargs())
    except Exception as exc:
        sys.exit(connect_error(exc))

    problems = []
    stats = {r: Counter() for r in args.requests}
    started = time.monotonic()
    for request_type in args.requests:
        for i in range(args.count):
            data = random_request(rng, request_type)
            payload = {"vendorName": args.vendor, "requestType": request_type}
            if data is not None:
                payload["requestData"] = data
            try:
                response = client.send("CallVendorRequest", payload, raw=True)["responseData"]
            except OBSSDKRequestError as exc:
                # obs-websocket itself rejected the request (e.g. requestData not an object)
                stats[request_type]["rejected by obs-websocket"] += 1
                continue
            except Exception as exc:
                problems.append(f"{request_type} #{i}: {type(exc).__name__}: {exc}"[:300])
                break
            if not well_formed(response):
                problems.append(f"{request_type} #{i}: malformed response {json.dumps(response)[:200]}")
            elif response["ok"]:
                stats[request_type]["ok"] += 1
            else:
                stats[request_type]["error: " + response["error"].split("'")[0].strip()[:45]] += 1

    try:
        info = client.send("CallVendorRequest", {"vendorName": VENDOR, "requestType": "GetInfo"}, raw=True)
        alive = info["responseData"].get("ok") is True
    except Exception as exc:
        alive = False
        problems.append(f"GetInfo after fuzzing failed: {exc}")
    client.disconnect()

    for request_type, counter in stats.items():
        print(f"{request_type}: " + ", ".join(f"{k} x{v}" for k, v in counter.most_common(8)))
    print(f"{args.count * len(args.requests)} requests in {time.monotonic() - started:.0f} s; "
          f"OBS still answering: {alive}; problems: {len(problems)}")
    for p in problems[:20]:
        print("PROBLEM", p)
    return 0 if alive and not problems else 1


if __name__ == "__main__":
    sys.exit(main())
