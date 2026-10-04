"""Rate limiting of websocket RunCommand (M5): 30/s per owner, burst 60.

Prerequisites: as test_vendor.py (ws-companion.lua loaded); hello-bridge.lua
loaded too (the "other owner" check). Reads the test OBS log to check that
rate limiting is logged once and then summarised, not flooded.

  OBS_LOG_DIR  default C:/obs-test/config/obs-studio/logs

Run:  py tests/websocket/test_rate_limit.py   (takes about 15 s)
"""

import os
import pathlib
import re
import sys
import time
import unittest

import obsws_python as obsws

from test_vendor import call, connect_error, connect_kwargs

LOG_DIR = pathlib.Path(os.environ.get("OBS_LOG_DIR", "C:/obs-test/config/obs-studio/logs"))
LIMIT_LINE = re.compile(r"\[lua-bridge\] websocket RunCommand\(wstest\): rate limited \(30/s, burst 60\)(.*)$")


def run(client, owner, command):
    return call(client, "RunCommand", {"owner": owner, "command": command})


class Log:
    """Reads lines appended to the newest OBS log since this object was made."""

    def __init__(self):
        logs = sorted(LOG_DIR.glob("*.txt"), key=lambda p: p.stat().st_mtime)
        if not logs:
            raise unittest.SkipTest(f"no OBS log in {LOG_DIR} (set OBS_LOG_DIR)")
        self.path = logs[-1]
        self.offset = self.path.stat().st_size

    def new_lines(self):
        with open(self.path, "rb") as f:
            f.seek(self.offset)
            return f.read().decode("utf-8", errors="replace").splitlines()


class RateLimitTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            cls.client = obsws.ReqClient(**connect_kwargs())
        except Exception as exc:
            raise unittest.SkipTest(connect_error(exc)) from None
        owners = {o["owner"] for o in call(cls.client, "ListOwners").get("owners", [])}
        if not {"wstest", "hello"} <= owners:
            cls.client.disconnect()
            raise unittest.SkipTest("load tests/websocket/ws-companion.lua and lua/examples/hello-bridge.lua")

    @classmethod
    def tearDownClass(cls):
        cls.client.disconnect()

    def ticks(self):
        return call(self.client, "GetState", {"owner": "wstest"})["state"].get("ticks", 0)

    def burst(self, count):
        accepted, limited = 0, []
        for _ in range(count):
            response = run(self.client, "wstest", "tick")
            if response.get("accepted"):
                accepted += 1
            else:
                limited.append(response)
        return accepted, limited

    def test_rate_limit(self):
        time.sleep(2.5)  # let the owner's bucket refill from earlier suites
        log = Log()
        start = self.ticks()

        accepted, limited = self.burst(200)
        self.assertGreaterEqual(accepted, 60, "the full burst is accepted")
        self.assertLess(accepted, 90, "the rest is rate limited")
        self.assertEqual(accepted + len(limited), 200)
        for response in limited:
            self.assertIs(response["ok"], False)
            self.assertRegex(response["error"], r"^rate limited; retry in \d+ ms$")
            self.assertIsInstance(response["retry_after_ms"], int)
            self.assertGreater(response["retry_after_ms"], 0)
            self.assertNotIn("accepted", response)

        # Another owner isn't affected
        self.assertEqual(run(self.client, "hello", "ping"), {"ok": True, "accepted": True})

        # Every accepted command was delivered, none of the limited ones
        deadline = time.monotonic() + 5
        while self.ticks() != start + accepted and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertEqual(self.ticks(), start + accepted)

        first = [m for m in (LIMIT_LINE.search(line) for line in log.new_lines()) if m]
        self.assertEqual(len(first), 1, "one warning for the whole burst")
        self.assertEqual(first[0].group(1), "", "no summary in the first warning")

        # After the 10 s window, the next rejection logs again with a summary
        time.sleep(10.5)
        accepted2, limited2 = self.burst(120)
        self.assertGreater(len(limited2), 0)
        lines = [m for m in (LIMIT_LINE.search(line) for line in log.new_lines()) if m]
        self.assertEqual(len(lines), 2, "one more line after 10 s")
        self.assertRegex(lines[1].group(1), rf"^; {len(limited) - 1} more requests were rate limited in the last 10 s$")


if __name__ == "__main__":
    result = unittest.main(exit=False, verbosity=2).result
    if result.skipped and len(result.skipped) >= result.testsRun:
        sys.exit(2)
    sys.exit(0 if result.wasSuccessful() else 1)
