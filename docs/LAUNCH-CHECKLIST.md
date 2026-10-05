# Launch checklist: before making the repository public

Work through this before switching `midnight-studios/obs-lua-bridge` to public and publishing 0.9.0-beta1.
Items marked **(maintainer)** are GitHub settings or decisions; the rest were checked from the repo.

## 1. Secrets and personal data scan
*Run on 2026-10-05 on branch `m7-launch-prep` (before the M7 commit), and against a fresh clone of the GitHub repo.*

**Commit identities** (fresh clone of the GitHub repo, all branches: 52 commits):

| Author \| committer | Commits |
|---|---|
| `GrumpyDog <25770335+midnight-studios@users.noreply.github.com>` (both) | 41 |
| `midnight-studios <25770335+midnight-studios@users.noreply.github.com>` \| `GitHub <noreply@github.com>` | 11 (PR merges) |

**No real email address** appears in any commit identity. The pre-rewrite history with the old address lives
only in the private, archived `obs-lua-bridge-dev`.
- Local clones made before the switch may still hold stale remote-tracking refs to it.
- Run `git remote prune origin` in them. This was done in the maintainer's working copy on 2026-10-05.

**Pattern search** over the full history of every commit and over the working tree, including the M7 files:
- **The patterns:** email addresses, the maintainer's real name and former email address (the exact search
  terms are kept out of the repo), `C:\Users\`, `/Users/<name>`, `AppData`, the Windows short user path,
  private IPs (`10.x`, `172.16–31.x`, `192.168.x`), GitHub/AWS/Slack token formats, private keys, and
  `password=`/`token=` literals.
- **The findings**, none of them a problem:

| Finding | Where | Verdict |
|---|---|---|
| `stephane.lepin@gmail.com`, `tt2468@gmail.com` | `src/third-party/obs-websocket-api.h` copyright header | OK: the obs-websocket authors' published notice, which must be kept |
| `25770335+midnight-studios@users.noreply.github.com` | `buildspec.json` (plugin metadata) | OK: GitHub noreply address |
| `me@example.com` | `buildspec.json` in early template commits | OK: template placeholder |
| `APPDATA` | `tools/deploy-test.ps1` | OK: an environment variable name, not a path |
| `10.0.20348.0` | `cmake/windows/compilerconfig.cmake` | OK: the Windows SDK version, not an IP |
| `C:\Users\`, `AppData` | `docs/M7-PLAN.md` | OK: describes this scan |
| `GrumpyDog` | `docs/SPEC.md` (author line, sample `git config user.name`), commit author name | OK: the author handle, kept by decision; never in a path |

**gitleaks**, run once locally (not in CI), version 8.30.1, official release binary with its checksum verified:
```
gitleaks git <fresh clone of github.com/midnight-studios/obs-lua-bridge> --log-opts="--all" --no-banner --redact
  → 41 commits scanned (merge commits skipped), ~715 KB: no leaks found (exit 0)
gitleaks dir <copy of the working tree: tracked + new files, without build_* and .deps> --no-banner --redact
  → 170 files, ~721 KB: no leaks found (exit 0)
```

- [x] Scan done; nothing to remove
- [x] **(maintainer)** Re-run if anything is committed between this PR and going public:
  `gitleaks git . --log-opts="--all" --redact`. **Re-run on 2026-10-05,** after PRs #2–#4 were merged; see
  below.

### Re-run on 2026-10-05 (head of the launch-settings PR)
**Commit identities** (fresh clone, all 5 branches, 60 commits):
- `GrumpyDog` with the noreply address: 45;
- `midnight-studios` with the noreply address, committed by `GitHub <noreply@github.com>`: 14 (PR merges);
- `dependabot[bot]` with its noreply address: 1.

No real email address.

**gitleaks 8.30.1:**
```
gitleaks git <fresh clone, --log-opts="--all"> --no-banner --redact   → 46 commits: no leaks found (exit 0)
gitleaks dir <PR head working tree, 172 files> --no-banner --redact   → no leaks found (exit 0)
```

**Pattern search:** one finding, introduced by this checklist itself.
- **The finding:** the first version of section 1, in the M7 commit `5d282f8`, listed the maintainer's first
  name and surname as search terms.
- **The fix:** this PR removes them from the file, and the exact terms are now kept out of the repo.
- **What remains:** they **remain in the git history** of `master` (from `5d282f8`, merged with PR #2) and in
  the pull-request refs of PRs #2, #4 and #5.
- **Whether to purge them** (history rewrite, or GitHub Support) **is the maintainer's decision.**
- **Every other finding** is the same as above: third-party notices, the noreply address, placeholders, the
  SDK version, and the scan's own descriptions.

## 2. License files
- [x] `LICENSE`: GPL-2.0, for the plugin
- [x] `lua/LICENSE`: MIT, for the helper and examples. The full MIT text is also in `lua/luabridge.lua`, and SPDX
  headers are in every file under `lua/`.
- [x] `THIRD-PARTY-NOTICES.md`: obs-websocket API header (GPL-2.0+) and nlohmann/json (MIT, full text)
- [x] Every package ships `LICENSE` and `THIRD-PARTY-NOTICES.md`, plus `lua/LICENSE`. Verified in CI artifacts:
  - Windows zip: `obs-lua-bridge/`;
  - `.deb`/tarball: `share/doc/obs-lua-bridge/`;
  - macOS: `Contents/Resources/`.
- [x] Each release attaches the source tarball (GPL-2.0 §3a)
- [x] `SECURITY.md`, `CONTRIBUTING.md`, `CHANGELOG.md` present

## 3. GitHub settings to enable once public (maintainer)
Branch protection and rulesets aren't available for private repositories on the free plan, so these wait for
the switch.

*Checked on 2026-10-05 after the switch to public:*
- **Without a token:** the visibility and the rules that apply to `master`, via
  `GET /repos/…/obs-lua-bridge` and `GET /repos/…/rules/branches/master`;
- **with the maintainer's `gh` login:** the ruleset's bypass list and the security settings, which GitHub only
  shows to admins.

**Settings → Rules → Rulesets → New branch ruleset**, target `master`, enforcement **Active**.
Ruleset "protect master" (id 24481055), active, targeting the default branch (`master`):
- [x] Require a pull request before merging; required approvals **0** while there's one maintainer
- [x] Require status checks to pass:
  - `Unit Tests 🧪 (ubuntu-24.04)`, `Unit Tests 🧪 (macos-15)`, `Unit Tests 🧪 (windows-2022)`;
  - `Build for Ubuntu 🐧 (ubuntu-24.04)`, `Build for macOS 🍏`, `Build for Windows 🪟`;
  - `clang-format`, `gersemi`.

  All 8 are required, under their full CI names (e.g. `Unit Tests 🧪 / Unit Tests 🧪 (macos-15)`).
- [x] Block force pushes (`non_fast_forward`)
- [x] Restrict deletions (`deletion`)
- [x] Bypass list: repository role **Admin**, mode "always" (the maintainer's emergency bypass)

**Settings → Code security:**
- [x] **Private vulnerability reporting: enabled.** `SECURITY.md` and the issue template link to it.
- [x] **Dependabot alerts: enabled.**
- [x] **Dependabot security updates: enabled.**
- [x] Dependabot version updates: already configured in `.github/dependabot.yml` (GitHub Actions, monthly)
- [x] Secret scanning: enabled.
- [ ] **Push protection: still disabled.** Re-checked on 2026-10-05 after the maintainer reported enabling it:
  the repository API still says `secret_scanning_push_protection: disabled`, while secret scanning says
  `enabled`. The setting is per repository, in this repo's Settings → Code security → Secret Protection →
  Push protection. The personal setting "Push protection for yourself" is a different switch.

**Settings → Actions → General:**
- [x] Actions enabled, all actions allowed (as today)
- [x] Workflow permissions: **read repository contents** (the default token is read-only; jobs that need more
  request it)
- [x] "Allow GitHub Actions to create and approve pull requests": **off**
- [x] Fork pull request workflows: **Require approval for first-time contributors** (confirmed:
  `approval_policy: first_time_contributors`)

**Settings → General:**
- [x] Description set: "Lets OBS Lua scripts register commands, publish state, add dock controls and talk to
  each other and to obs-websocket."
- [ ] Website: the OBS forum resource page (after it's posted). It currently points to the maintainer's OBS
  forum profile; switch it once the resource page exists.
- [x] Topics: `lua`, `obs-plugin`, `obs-studio`, `obs-websocket`, `streaming`
- [x] Issues on, wiki off, discussions off (as today)
- [x] Issue templates: `.github/ISSUE_TEMPLATE/`. The bug report asks for the OBS version, OS, plugin version,
  install method, steps and the log.

## 4. Release readiness
- [x] **Versions agree:**
  - `buildspec.json` `"version": "0.9.0-beta1"`;
  - the tag will be `0.9.0-beta1`, which `push.yaml` marks as a pre-release, with the tag check matching;
  - the plugin reports `0.9.0-beta1` in GetInfo (Lua and websocket) and in the OBS log.

  Verified on the test OBS on 2026-10-05.
- [x] `CHANGELOG.md` has the `0.9.0-beta1` entry, and `docs/RELEASING.md` describes the steps
- [x] **(maintainer)** The M7 PR merged, with CI green on `master` (`c17700a`, and every `master` push since,
  up to `ddc4537`)

## 5. Go / no-go: in this order, all decisions by the maintainer
1. [ ] Sections 1–4 done; nothing committed since the scan, or the scan re-run. **Not ticked:** the re-run found
   the maintainer's name in the history (section 1), and push protection is still off (section 3).
2. [x] **Make the repository public** (Settings → General → Danger Zone → Change visibility). Confirmed
   without a token: `visibility: public`.
3. [x] Apply section 3's settings: the ruleset, private vulnerability reporting, Dependabot alerts. Still open in
   section 3: push protection, and the website link.
4. [ ] **Tag `0.9.0-beta1`** on `master` and push it (`docs/RELEASING.md`). CI creates the draft pre-release.
5. [ ] Check the draft: install the Windows zip and see `version 0.9.0-beta1` in the log; the files are all there
6. [ ] Publish the release
7. [ ] Post the forum resource (`docs/launch/forum-resource.md`), then send the messages to Exeldro and the OBS
   team (`docs/launch/`)
8. [x] Add the dock screenshot to the README (`docs/images/dock.png`; checked: nothing personal visible, no embedded metadata, 43 KB)
