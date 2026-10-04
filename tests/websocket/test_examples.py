"""Websocket tests for the Score Board and Stopwatch examples (M4).

Prerequisites (besides those of test_vendor.py): load
lua/examples/scoreboard.lua and lua/examples/stopwatch-demo.lua in
Tools > Scripts of the test OBS. Commands are addressed by owner id only
("scoreboard", "stopwatch"), never by script filename.

Run:  py tests/websocket/test_examples.py   (repeatable; restores what it changes)
"""

import sys
import time
import unittest

import obsws_python as obsws

from test_vendor import Events, call, connect_error, connect_kwargs


def run(client, owner, command, data=None):
    request = {"owner": owner, "command": command}
    if data is not None:
        request["data"] = data
    return call(client, "RunCommand", request)


class ExampleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            cls.client = obsws.ReqClient(**connect_kwargs())
            cls.events = Events()
        except Exception as exc:
            raise unittest.SkipTest(connect_error(exc)) from None
        owners = {o["owner"] for o in call(cls.client, "ListOwners").get("owners", [])}
        missing = {"scoreboard", "stopwatch"} - owners
        if missing:
            cls.tearDownClass()
            raise unittest.SkipTest(
                "load lua/examples/scoreboard.lua and lua/examples/stopwatch-demo.lua in Tools > Scripts "
                f"(not registered: {', '.join(sorted(missing))})"
            )

    @classmethod
    def tearDownClass(cls):
        if getattr(cls, "events", None):
            cls.events.close()
        if getattr(cls, "client", None):
            cls.client.disconnect()

    def setUp(self):
        self.events.clear()

    def state(self, owner):
        response = call(self.client, "GetState", {"owner": owner})
        self.assertTrue(response["ok"], response)
        return response["state"]

    def wait_for(self, owner, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        state = None
        while time.monotonic() < deadline:
            state = self.state(owner)
            if predicate(state):
                return state
            time.sleep(0.05)
        self.fail(f"{owner} state never matched; last: {state}")

    def assert_accepted(self, response):
        self.assertEqual(response, {"ok": True, "accepted": True})

    # Score Board ---------------------------------------------------------

    def test_scoreboard_state_and_events(self):
        start = self.state("scoreboard")
        home, away = start.get("home", 0), start.get("away", 0)

        self.assert_accepted(run(self.client, "scoreboard", "home_plus"))
        self.assert_accepted(run(self.client, "scoreboard", "away_plus"))
        self.assert_accepted(run(self.client, "scoreboard", "away_plus"))
        state = self.wait_for("scoreboard", lambda s: s.get("home") == home + 1 and s.get("away") == away + 2)
        self.assertEqual(state["display"], f"{home + 1} – {away + 2}")

        event = self.events.wait_for(
            lambda t, d: t == "CustomEvent"
            and d.get("owner") == "scoreboard"
            and d.get("event") == "score.changed"
            and d.get("data") == {"home": home + 1, "away": away + 2},
            2,
        )
        self.assertIsNotNone(event, "no score.changed CustomEvent with the final score")
        changed = self.events.wait_for(
            lambda t, d: t == "StateChanged" and d.get("owner") == "scoreboard" and "display" in d.get("changes", {}),
            2,
        )
        self.assertIsNotNone(changed, "no StateChanged for the score board")

        # Restore the previous score (minus never goes below 0)
        self.assert_accepted(run(self.client, "scoreboard", "home_minus"))
        self.assert_accepted(run(self.client, "scoreboard", "away_minus"))
        self.assert_accepted(run(self.client, "scoreboard", "away_minus"))
        self.wait_for("scoreboard", lambda s: s.get("home") == home and s.get("away") == away)

    def test_scoreboard_rejects_unknown_commands(self):
        response = run(self.client, "scoreboard", "home_plus_two")
        self.assertIs(response["ok"], False)
        self.assertIn("unknown command", response["error"])

    # Stopwatch -----------------------------------------------------------

    def test_stopwatch_by_owner(self):
        self.assert_accepted(run(self.client, "stopwatch", "reset"))
        state = self.wait_for("stopwatch", lambda s: s.get("elapsed") == 0 and s.get("running") is False)
        self.assertEqual(state["display"], "00:00:00")
        self.assertNotIn("toggle_label", state, "reset shows the default 'Start' label")

        self.assert_accepted(run(self.client, "stopwatch", "add", {"seconds": 75}))
        state = self.wait_for("stopwatch", lambda s: s.get("elapsed") == 75)
        self.assertEqual(state["display"], "00:01:15")
        self.assertEqual(state["toggle_label"], "Resume")

        self.assert_accepted(run(self.client, "stopwatch", "subtract", {"seconds": 100}))
        self.wait_for("stopwatch", lambda s: s.get("elapsed") == 0)

        self.assert_accepted(run(self.client, "stopwatch", "toggle"))
        state = self.wait_for("stopwatch", lambda s: s.get("running") is True)
        self.assertEqual(state["toggle_label"], "Pause")
        self.wait_for("stopwatch", lambda s: s.get("elapsed", 0) >= 1, timeout=4)

        self.assert_accepted(run(self.client, "stopwatch", "pause"))
        paused = self.wait_for("stopwatch", lambda s: s.get("running") is False)
        time.sleep(1.2)
        self.assertEqual(self.state("stopwatch")["elapsed"], paused["elapsed"], "time doesn't advance while paused")

        self.assert_accepted(run(self.client, "stopwatch", "reset"))
        self.wait_for("stopwatch", lambda s: s.get("elapsed") == 0 and s.get("running") is False)

    def test_stopwatch_validates_args(self):
        response = run(self.client, "stopwatch", "add", {"seconds": "ten"})
        self.assertIs(response["ok"], False)
        self.assertIn("argument 'seconds' must be int", response["error"])


if __name__ == "__main__":
    result = unittest.main(exit=False, verbosity=2).result
    if result.skipped and len(result.skipped) >= result.testsRun:
        sys.exit(2)
    sys.exit(0 if result.wasSuccessful() else 1)
