# M7: Launch prep (plan)

**Status:** approved 2026-10-05, with three additions (below).
**Branch:** `m7-launch-prep`, from `master` at `fc24ecd`.

## Additions approved with the plan
1. **Version strings must all agree.** They didn't: buildspec, the plugin and the log said `0.9.0`, while the tag
   was to be `0.9.0-beta1`. Decision: **`0.9.0-beta1` everywhere.**

   | Where | Value |
   |---|---|
   | `buildspec.json` `"version"` | `0.9.0-beta1` |
   | Tag | `0.9.0-beta1`. `push.yaml`'s `X.Y.Z-(beta\|rc)N` pattern marks it a **pre-release**; `build-project.yaml` makes it a Release build; the tag check now requires tag == buildspec version exactly. |
   | GetInfo `plugin_version` (Lua procedure and websocket) | `0.9.0-beta1` (verified on the test OBS) |
   | OBS log | `[lua-bridge] plugin loaded successfully (version 0.9.0-beta1)` (verified) |
   | Dock | shows no version |
   | Package names | `obs-lua-bridge-0.9.0-beta1-…` |
   | Numeric-only fields | Windows file version `0.9.0.0` (product version text `0.9.0-beta1`), macOS bundle version `0.9.0`, `.deb` version `0.9.0~beta1` |
   | `api_version` | stays the integer `1`; the helper only compares that |

   **Implementation:**
   - `cmake/common/bootstrap.cmake` validates the version and keeps the full string as `PLUGIN_VERSION`. The numeric
     part goes to `project()` and `PLUGIN_VERSION_NUMERIC`.
   - `plugin-support.c.in` uses the full version. The platform version fields and CPack were adjusted.
   - **Tests:** a Lua unit test proves a pre-release `plugin_version` is accepted, and `test_vendor.py` requires GetInfo
     to equal `buildspec.json`.
2. **gitleaks run once locally;** the command and result are recorded in `docs/LAUNCH-CHECKLIST.md`.
3. **`SECURITY.md`** (private vulnerability reporting), plus "enable private vulnerability reporting" in the
   checklist's post-public settings.

## Context
- M6 made the project releasable: CI tests, packages for three platforms, the release flow, and the history rewrite.
- PR #1 added the ping-pong example.
- M7 prepares the **0.9.0-beta1** launch:
  - documentation written for end users;
  - research into which control surfaces can drive the plugin;
  - the release notes;
  - a pre-public checklist;
  - drafts of the announcement texts.

**Two items carried over from PR #1 are already on `master`** (checked; nothing to add in M7):
- **The fire-and-forget note:** `docs/API.md:56`, and `lua/examples/ping-pong/README.md:27-31`.
- **The v1.1 idea "Lua request/reply helper or read-only `get_state(owner)`":** `docs/SPEC.md:426` (Part G), and `CHANGELOG.md:16`.

**Rules for this milestone:**
- one branch and one PR;
- commits batched, with a single push;
- no tags, no visibility changes;
- nothing posted outside the repo;
- the existing tests run before the PR, with their results reported.

---

## 1. User docs (me, ~3 h)
**`README.md`**, rewritten for streamers and script authors, not developers:
- **What it does:** one paragraph, plus a screenshot placeholder
  (`docs/images/dock.png`, *to be added*: I can't take screenshots of your OBS; see "What needs you").
- **Install:** short steps per platform, linking to `docs/INSTALL.md` for details.
  - **Windows:** the zip → `C:\ProgramData\obs-studio\plugins\`.
  - **macOS:** `.pkg` with the manual **Open Anyway** approval (Privacy & Security), or the `.tar.xz` with `xattr -dr com.apple.quarantine`. A clear note that the build isn't signed or notarized during the beta.
  - **Linux:** `.deb` for Ubuntu 24.04, or the `.tar.xz` for other distributions.
- **5-minute quick start with `hello-bridge.lua`:**
  1. install;
  2. Tools → Scripts → + → the shipped `lua/examples/hello-bridge.lua` (its path for each platform);
  3. Docks → Lua Bridge → **Ping**;
  4. see "Last ping" update and the log line;
  5. next steps: copy `luabridge.lua` next to your own script.
- **Examples:** one line each for `hello-bridge`, `scoreboard`, `stopwatch-demo` and `ping-pong` (scripts talking to each other).
- **Links:** `docs/API.md`, `docs/INSTALL.md`, `lua/README.txt`, `CHANGELOG.md`, and the control-surface page (item 2).
- **Footer:** license (plugin GPL-2.0+, `lua/` MIT) and the beta notice.

**New `CONTRIBUTING.md`:** the developer material moves out of the README:
- building (presets, the plugin template wiki);
- the test suites (pointing to `docs/TESTING.md`);
- formatting (`clang-format`, `gersemi`; note the gersemi version CI uses);
- branches and PRs, the "Seeking Testers" label;
- releasing (`docs/RELEASING.md`);
- licensing of contributions.

**Accuracy pass (me):** every command, file name and path in README, INSTALL, `lua/README.txt` and CONTRIBUTING is checked against the **actual CI packages**:
- I download the artifacts of the latest `master` run (or this PR's, labelled "Seeking Testers") and list each package.
- Every path the docs mention must exist in it: zip layout, `.deb` install paths, `.pkg` payload, tarball contents, and the file names including the version.
- The findings go in the PR description.

## 2. Control surfaces: research only, no code (me, ~2 h)
- **Question:** can each tool send an obs-websocket **5.x** `CallVendorRequest` with `vendorName: "LuaBridge"`, `requestType: "RunCommand"` and `requestData: {owner, command, data}`?
- **Tools:**
  1. **Elgato Stream Deck**: the official OBS Studio plugin;
  2. **Bitfocus Companion**: the `obs-studio` module;
  3. **Touch Portal**: its OBS integration;
  4. **Streamer.bot**: its OBS actions.
- **Method:**
  - read each tool's official docs, release notes and source code, where it's open (Companion's module and Streamer.bot's docs especially);
  - look for a generic or "raw" request action, vendor requests, or custom JSON;
  - for each tool, record: **supported / not supported / unknown**, how it's configured (the action name and the JSON to enter), the version checked, and a **source link**;
  - anything I can't confirm from a primary source is marked **unverified**.
- **Fallback when a tool can't send vendor requests:** note any indirect route, for example a hotkey or a Lua script the tool can trigger. No Stream Deck plugin gets built.
- **Output:** `docs/integrations/control-surfaces.md`, linked from the README.
- I'll use web search and fetch for this. Nothing is posted anywhere.

## 3. Release prep for 0.9.0-beta1 (me, ~1 h)
- **`CHANGELOG.md`:** move "Unreleased" into `## 0.9.0-beta1 (date set at tagging)`, and start an empty "Unreleased". The "Ideas for v1.1" section stays.
- **`docs/RELEASING.md`:**
  - the beta-specific steps: tag `0.9.0-beta1` on `master` → draft pre-release → check it (the install test) → publish;
  - the order relative to making the repo public: **make it public first**, so the release, the source and the GPL offer are visible together;
  - how to write the release text from the CHANGELOG entry.
- **Consistency check, reported:**
  - `buildspec.json` `"version": "0.9.0"` matches the tag `0.9.0-beta1`;
  - the release job's tag patterns accept `-beta1`, and its tag-vs-buildspec check passes;
  - the package names CI produces match what `docs/INSTALL.md` says;
  - `plugin_version` in `docs/API.md` matches.

  **No tag is created or pushed.**

## 4. Pre-public checklist: `docs/LAUNCH-CHECKLIST.md` (me: scans and the document; you: ticking the GitHub settings)
- **Scans, run now, results recorded in the checklist:**
  - **Working tree and the full history** of the new repo (every commit) for:
    - email addresses (anything other than the noreply address and third-party copyright lines);
    - `C:\Users\…`, `GrumpyDog` in paths (the author name GrumpyDog is allowed), and `AppData`;
    - passwords, tokens and keys;
    - local IPs.
  - **`gitleaks`** over the full history, and over the working tree in no-git mode.
  - Each finding is listed with its file and line, and whether it's acceptable (e.g. the obs-websocket authors' copyright email).
- **License files:** `LICENSE` (GPL-2.0), `lua/LICENSE` (MIT), `THIRD-PARTY-NOTICES.md`; the headers in `lua/`; all shipped in every package (checked via the CI artifacts).
- **To enable once public (you):**
  - **Branch protection or a ruleset on `master`:**
    - require a PR;
    - require the status checks to pass: Unit Tests ×3, the three builds, clang-format, gersemi;
    - block force-pushes and deletion;
    - optionally require a linear history (no).
  - **Private vulnerability reporting.**
  - **Dependabot:** alerts and security updates on. Version updates are already configured in `.github/dependabot.yml`.
  - **Actions:** keep the token read-only and "allow all actions", or restrict to GitHub- and verified-creator actions. Fork PR workflows should require approval for first-time contributors.
  - **Repo settings:** description, topics (`obs-studio`, `obs-plugin`, `lua`, `obs-websocket`, `stream-deck`), website link.
- **Issue templates (me, new files):**
  - `.github/ISSUE_TEMPLATE/bug_report.yml` (a form): OBS version, OS and version, plugin version (from the log line or `GetInfo`), steps, expected vs actual, and the log file (a link to Help → Log Files → Upload Current Log File);
  - `feature_request.yml`;
  - `config.yml` linking to the docs.

  These are files in the repo, not settings, so they're part of this PR.
- **Final go/no-go list:** everything that has to be true before you flip the visibility, including "PR #… merged", "CI green on master", and "draft release checked".

## 5. Drafts under `docs/launch/`, never posted (me)
- **`forum-resource.md`:** the OBS forum resource text:
  - an overview;
  - features: dock controls, script-to-script commands and events, obs-websocket integration, works without the plugin;
  - install (short, linking to INSTALL);
  - compatibility: OBS 31.1+, Windows x64, macOS universal (unsigned), Ubuntu 24.04 x86_64, other Linux via the tarball;
  - a **beta notice**;
  - a **"Mac testers wanted"** call (what to test, and how to report);
  - credits, including Exeldro for the original idea.
- **`message-exeldro.md`:** a short thank-you for the original idea, a link to the repo and the API, and an invitation to give feedback.
- **`note-obs-team.md`:** a short note: what it is, the open repo, the API surface (procedures, signals, websocket vendor), and openness to feedback and upstreaming.
- All three drafts get placeholders for links that only exist after launch (release URL, forum thread). Each is marked **DRAFT, not posted**.

## What needs you
- **Approve this plan.**
- **A dock screenshot** and, optionally, a short GIF for the README and the forum. A placeholder stays until you provide them, or I write exact capture instructions.
- After merging: the GitHub settings in item 4, when the repo goes public.
- **When to tag** `0.9.0-beta1`, and when to post the drafts. Both are always your call.

## Order of work
1. Items 1–5 as listed, with the scans (item 4) run on the final tree just before committing.
2. **Existing tests:**
   - `py tests/lua-unit/test_luabridge.py` and `ctest`;
   - on the test OBS: `test_vendor.py`, `test_examples.py`, `test_rate_limit.py`, `test_ping_pong.py` (and `--close`), plus the harness RESULT lines.

   This milestone changes no code, but the suites confirm the docs describe working behaviour.
3. One commit batch → **one push** → the PR, with the "Seeking Testers" label so the accuracy pass can use this PR's packages.
4. Report every CI check. The PR is not ready until all are green.

## Out of scope
- Any code or API change.
- A Stream Deck plugin.
- Tags or releases.
- Making the repo public.
- Posting the drafts.
- macOS signing.
