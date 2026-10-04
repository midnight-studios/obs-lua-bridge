"""Estimates this month's billed GitHub Actions minutes for this repo.

Sums every job of every workflow run created this month (UTC), rounding each
job up to whole minutes and applying GitHub's multipliers for private repos
(Linux x1, Windows x2, macOS x10). An estimate: the exact figure is on
GitHub's billing page (or `gh api /users/<owner>/settings/billing/usage` with
the "user" token scope). Other repos of the same account share the quota.

Run:  py tools/actions-usage.py [--repo owner/name] [--warn 1500]
"""

import argparse
import datetime
import json
import math
import subprocess
import sys

MULTIPLIER = {"ubuntu": 1, "windows": 2, "macos": 10}


def gh(path):
    out = subprocess.run(["gh", "api", "--paginate", path], capture_output=True, encoding="utf-8",
                         check=True).stdout
    # --paginate concatenates JSON documents; split them
    decoder, pos, docs = json.JSONDecoder(), 0, []
    while pos < len(out):
        while pos < len(out) and out[pos].isspace():
            pos += 1
        if pos >= len(out):
            break
        doc, pos = decoder.raw_decode(out, pos)
        docs.append(doc)
    return docs


def parse(ts):
    return datetime.datetime.fromisoformat(ts.replace("Z", "+00:00")) if ts else None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default="midnight-studios/obs-lua-bridge")
    parser.add_argument("--warn", type=int, default=1500)
    args = parser.parse_args()

    now = datetime.datetime.now(datetime.timezone.utc)
    month_start = now.replace(day=1, hour=0, minute=0, second=0, microsecond=0)
    runs = [r for doc in gh(f"/repos/{args.repo}/actions/runs?per_page=100&created=>={month_start:%Y-%m-%d}")
            for r in doc["workflow_runs"]]

    billed = {k: 0 for k in MULTIPLIER}
    for run in runs:
        for doc in gh(f"/repos/{args.repo}/actions/runs/{run['id']}/jobs?per_page=100"):
            for job in doc["jobs"]:
                start, end = parse(job.get("started_at")), parse(job.get("completed_at"))
                if not start or not end or end <= start:
                    continue
                labels = " ".join(job.get("labels", [])).lower()
                os_name = next((k for k in MULTIPLIER if k in labels), "ubuntu")
                billed[os_name] += math.ceil((end - start).total_seconds() / 60) * MULTIPLIER[os_name]

    total = sum(billed.values())
    print(f"{now:%B %Y}: {len(runs)} runs, about {total} billed minutes "
          f"(Linux {billed['ubuntu']}, Windows {billed['windows']}, macOS {billed['macos']})")
    if total > args.warn:
        print(f"WARNING: over {args.warn} minutes")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
