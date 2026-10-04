"""Integration tests for the Lua Bridge obs-websocket vendor API (spec B8).

Prerequisites:
  * The test OBS is running with the plugin deployed and the WebSocket server
    enabled (Tools > WebSocket Server Settings).
  * tests/websocket/ws-companion.lua is loaded in Tools > Scripts.
  * pip install obsws-python

Connection settings come from the environment (never from files):
  OBS_WS_HOST      default "localhost"
  OBS_WS_PORT      default 4455
  OBS_WS_PASSWORD  the server password; leave unset if authentication is off

Run:  py tests/websocket/test_vendor.py
The suite is repeatable: run it as often as you like without restarting OBS.
"""

import json
import logging
import os
import sys
import threading
import time
import unittest

import obsws_python as obsws
from obsws_python.error import OBSSDKRequestError

# obsws-python logs the connection parameters, including the password, at INFO
# level, and logs a traceback for every failed request (some are expected
# here). Keep its logger quiet; the tests report failures themselves.
logging.getLogger("obsws_python").setLevel(logging.CRITICAL)

VENDOR = "LuaBridge"
OWNER = "wstest"
HOST = os.environ.get("OBS_WS_HOST", "localhost")
PORT = int(os.environ.get("OBS_WS_PORT", "4455"))
PASSWORD = os.environ.get("OBS_WS_PASSWORD", "")
# Unique per run, so state-changing tests always produce new values
RUN_ID = str(time.time_ns())


# Default RunCommand rate limit per owner (src/rate-limit.hpp)
RATE_BURST = 60


def paced(fn, count, per_second):
    """Calls fn count times, at most per_second times per second."""
    interval = 1.0 / per_second
    next_at = time.monotonic()
    for _ in range(count):
        delay = next_at - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        fn()
        next_at += interval


def connect_kwargs():
    # Always pass all three, so obsws-python never falls back to ~/config.toml
    return {"host": HOST, "port": PORT, "password": PASSWORD, "timeout": 5}


def connect_error(exc):
    hint = (
        "is the test OBS running with the WebSocket server enabled? If the server uses "
        "authentication, set OBS_WS_PASSWORD."
    )
    # Never include the password; only the exception type and message
    return f"cannot connect to obs-websocket at ws://{HOST}:{PORT} ({type(exc).__name__}: {exc}); {hint}"


class Events:
    """Collects LuaBridge vendor events from an EventClient."""

    def __init__(self):
        self._lock = threading.Lock()
        self._events = []
        self.client = obsws.EventClient(**connect_kwargs())

        def on_vendor_event(data):
            if data.vendor_name == VENDOR:
                with self._lock:
                    self._events.append((data.event_type, data.event_data))

        self._callback = on_vendor_event
        self.client.callback.register(on_vendor_event)

    def clear(self):
        with self._lock:
            self._events.clear()

    def wait_for(self, predicate, timeout):
        """Returns the first (type, data) matching predicate, or None."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                for event in self._events:
                    if predicate(*event):
                        return event
            time.sleep(0.05)
        return None

    def close(self):
        self.client.disconnect()


def call(client, request_type, data=None):
    """Calls a LuaBridge vendor request and returns its response data."""
    payload = {"vendorName": VENDOR, "requestType": request_type}
    if data is not None:
        payload["requestData"] = data
    response = client.send("CallVendorRequest", payload, raw=True)
    return response["responseData"]


def run(client, command, data=None):
    request = {"owner": OWNER, "command": command}
    if data is not None:
        request["data"] = data
    return call(client, "RunCommand", request)


class VendorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            cls.client = obsws.ReqClient(**connect_kwargs())
            cls.events = Events()
        except Exception as exc:  # connection refused, auth failure, timeout
            raise unittest.SkipTest(connect_error(exc)) from None

        owners = call(cls.client, "ListOwners").get("owners", [])
        if not any(o.get("owner") == OWNER for o in owners):
            cls.tearDownClass()
            raise unittest.SkipTest(
                f"owner '{OWNER}' is not registered: load tests/websocket/ws-companion.lua in Tools > Scripts"
            )

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "events", None):
            cls.events.close()
        if getattr(cls, "client", None):
            cls.client.disconnect()

    def setUp(self):
        self.events.clear()

    def state(self):
        response = call(self.client, "GetState", {"owner": OWNER})
        self.assertTrue(response["ok"], response)
        return response["state"]

    def assert_ok(self, response):
        self.assertTrue(response.get("ok"), response)

    def assert_error(self, response, substring):
        self.assertIs(response.get("ok"), False, response)
        self.assertIn(substring, response.get("error", ""), response)
        self.assertNotIn("accepted", response)

    def wait_for_state(self, key, expected, timeout=5):
        deadline = time.monotonic() + timeout
        value = None
        while time.monotonic() < deadline:
            value = self.state().get(key)
            if value == expected:
                return
            time.sleep(0.05)
        self.fail(f"state[{key!r}] is {value!r}, expected {expected!r}")

    # 1
    def test_get_info(self):
        info = call(self.client, "GetInfo")
        self.assert_ok(info)
        self.assertEqual(info["api_version"], 1)
        self.assertIs(info["capabilities"]["websocket"], True)
        self.assertIs(info["capabilities"]["run_command"], True)
        self.assertTrue(info["plugin_version"])
        self.assertTrue(info["obs_version"])

    # 2
    def test_list_owners(self):
        response = call(self.client, "ListOwners")
        self.assert_ok(response)
        owners = {o["owner"]: o for o in response["owners"]}
        self.assertEqual(owners[OWNER], {"owner": OWNER, "display_name": "WS Test Companion", "stale": False})
        ids = [o["owner"] for o in response["owners"]]
        self.assertEqual(ids, sorted(ids))

    # 3
    def test_list_commands(self):
        response = call(self.client, "ListCommands", {"owner": OWNER})
        self.assert_ok(response)
        self.assertEqual(response["owner"], OWNER)
        commands = {c["id"]: c for c in response["commands"]}
        self.assertEqual(list(commands), ["set", "del", "emit", "echo", "tick"])
        self.assertEqual(commands["set"]["args"], {"key": "string", "value": "string"})
        self.assertEqual(commands["echo"]["args"], {"i": "int", "n": "number", "s": "string", "b": "bool"})
        self.assertEqual(commands["tick"]["label"], "tick")
        self.assert_error(call(self.client, "ListCommands", {"owner": "nobody"}), "owner not registered")

    # 4 and 5
    def test_set_state_change_and_no_change(self):
        value = "red-" + RUN_ID
        self.assertEqual(run(self.client, "set", {"key": "color", "value": value}), {"ok": True, "accepted": True})
        event = self.events.wait_for(
            lambda t, d: t == "StateChanged" and d.get("owner") == OWNER and d["changes"].get("color") == value, 2
        )
        self.assertIsNotNone(event, "no StateChanged with the new color within 2 s")
        state = self.state()
        self.assertEqual(state["color"], value)
        self.assertEqual(state["last_origin"], "websocket")

        # Same value again: nothing changes, so no event
        self.events.clear()
        self.assert_ok(run(self.client, "set", {"key": "color", "value": value}))
        event = self.events.wait_for(lambda t, d: t == "StateChanged" and d.get("owner") == OWNER, 1)
        self.assertIsNone(event, f"unexpected StateChanged: {event}")

    def test_state_removal_is_reported(self):
        key = "tmp" + RUN_ID
        self.assert_ok(run(self.client, "set", {"key": key, "value": "x"}))
        self.wait_for_state(key, "x")
        self.events.clear()
        self.assert_ok(run(self.client, "del", {"key": key}))
        event = self.events.wait_for(
            lambda t, d: t == "StateChanged" and d.get("owner") == OWNER and key in d.get("removed", {}), 2
        )
        self.assertIsNotNone(event, "no StateChanged reporting the removed key within 2 s")
        self.assertEqual(event[1]["removed"], {key: True})
        self.assertEqual(event[1]["changes"], {})
        self.assertNotIn(key, self.state())

    # 6
    def test_custom_event(self):
        name = "ws.hello"
        self.assert_ok(run(self.client, "emit", {"event": name}))
        event = self.events.wait_for(lambda t, d: t == "CustomEvent" and d.get("event") == name, 2)
        self.assertIsNotNone(event, "no CustomEvent within 2 s")
        self.assertEqual(event[1], {"owner": OWNER, "event": name, "data": {"from": "wstest"}})

    def wait_for_echo(self, marker, timeout=5):
        """Returns the args the companion's echo command last received, once args["s"] == marker."""
        deadline = time.monotonic() + timeout
        received = None
        while time.monotonic() < deadline:
            raw = self.state().get("last_echo")
            received = json.loads(raw) if raw else None
            if received and received.get("s") == marker:
                return received
            time.sleep(0.05)
        self.fail(f"echo with s={marker!r} not received; last echo: {received!r}")

    def test_run_command_data_round_trip(self):
        # Every valid arg type reaches the script unchanged
        data = {"i": 1234567890123, "n": 0.1, "s": "café \U0001f3ae \"quoted\" \\ " + RUN_ID, "b": False}
        self.assert_ok(run(self.client, "echo", data))
        received = self.wait_for_echo(data["s"])
        self.assertEqual(received, data)
        self.assertIsInstance(received["i"], int)
        self.assertIsInstance(received["n"], float)

    def test_run_command_null_and_array_args(self):
        # null counts as an omitted arg and is removed before the script sees it
        # (obs-websocket 5.6 drops it itself; 5.7 passes it through to the plugin)
        marker = "null-" + RUN_ID
        self.assert_ok(run(self.client, "echo", {"s": marker, "i": None}))
        self.assertEqual(self.wait_for_echo(marker), {"s": marker})
        # A null for an undeclared name is ignored the same way
        marker = "null-undeclared-" + RUN_ID
        self.assert_ok(run(self.client, "echo", {"s": marker, "zzz": None}))
        self.assertEqual(self.wait_for_echo(marker), {"s": marker})
        # Arrays never reach a script as args: the key survives and fails validation
        self.assert_error(run(self.client, "echo", {"i": [1, 2]}), "argument 'i' must be int")
        self.assert_error(run(self.client, "echo", {"x": ["a"]}), "unknown argument 'x'")

    # 7
    def test_rejections(self):
        c = self.client
        cases = [
            ({"owner": "nobody", "command": "tick"}, "owner not registered"),
            ({"owner": "Bad Owner", "command": "tick"}, "invalid owner id"),
            ({"command": "tick"}, "missing owner"),
            ({"owner": 5, "command": "tick"}, "owner must be a string"),
            ({"owner": OWNER}, "missing command"),
            ({"owner": OWNER, "command": "nope"}, "unknown command 'nope'"),
            ({"owner": OWNER, "command": "set", "data": {"other": "x"}}, "unknown argument 'other'"),
            ({"owner": OWNER, "command": "set", "data": {"value": 1}}, "argument 'value' must be string"),
            ({"owner": OWNER, "command": "set", "data": "x"}, "data must be an object"),
            ({"owner": OWNER, "command": "set", "data": {"value": "x" * 70000}}, "json exceeds 65536 bytes"),
        ]
        for request, error in cases:
            with self.subTest(request=str(request)[:80]):
                self.assert_error(call(c, "RunCommand", request), error)

    # 8
    def test_get_state_unknown_owner(self):
        self.assert_error(call(self.client, "GetState", {"owner": "nobody"}), "owner not registered")
        self.assert_error(call(self.client, "GetState"), "missing owner")

    # 9
    def test_unknown_request_type(self):
        with self.assertRaises(OBSSDKRequestError):
            call(self.client, "NoSuchRequest")

    # 10
    def test_burst_is_lossless(self):
        # RunCommand is rate limited per owner (30/s, burst 60; see API.md): send
        # one full burst as fast as possible, then the rest just under the
        # sustained rate. Every command must be accepted and delivered.
        start = self.state().get("ticks", 0)
        for _ in range(RATE_BURST):
            self.assertEqual(run(self.client, "tick"), {"ok": True, "accepted": True})
        paced(lambda: self.assertEqual(run(self.client, "tick"), {"ok": True, "accepted": True}), 200 - RATE_BURST,
              per_second=25)
        self.wait_for_state("ticks", start + 200)

    # 11
    def test_concurrent_clients(self):
        start = self.state().get("ticks", 0)
        failures = []

        def worker():
            try:
                client = obsws.ReqClient(**connect_kwargs())
                try:
                    # 4 clients x 6/s stays under the per-owner limit (30/s)
                    def one():
                        response = run(client, "tick")
                        if response != {"ok": True, "accepted": True}:
                            failures.append(response)

                    paced(one, 50, per_second=6)
                finally:
                    client.disconnect()
            except Exception as exc:
                failures.append(f"{type(exc).__name__}: {exc}")

        threads = [threading.Thread(target=worker) for _ in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        self.assertEqual(failures, [])
        self.wait_for_state("ticks", start + 200)


if __name__ == "__main__":
    result = unittest.main(exit=False, verbosity=2).result
    if result.skipped and len(result.skipped) >= result.testsRun:
        sys.exit(2)  # nothing actually ran (OBS unreachable or companion not loaded)
    sys.exit(0 if result.wasSuccessful() else 1)
