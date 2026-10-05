# Releasing Lua Bridge for OBS

## Versions
- **The version lives in one place:** `"version"` in `buildspec.json`, for example `0.9.0-beta1` or `1.0.0`.
  - **The full string** is the plugin version: `plugin_version` in `luabridge_get_info` and websocket
    `GetInfo`, the OBS log line `[lua-bridge] plugin loaded successfully (version …)`, and the package file names.
  - **Only the numeric part** (`0.9.0`) goes where platforms require numbers: CMake's `project()`, the Windows
    file version, and the macOS bundle version.
  - **In the `.deb`** a pre-release is written `0.9.0~beta1`, so that dpkg sorts it before `0.9.0`.
  - **Allowed forms:** `X.Y.Z` or `X.Y.Z-<suffix>`. CMake configure fails on anything else.
- **Tags must equal the version exactly:** tag `0.9.0-beta1` needs `"version": "0.9.0-beta1"`, and tag `1.0.0`
  needs `"version": "1.0.0"`. The release job **fails** otherwise.
- **The tag decides the release type:**
  - `X.Y.Z` is a release;
  - `X.Y.Z-betaN` or `X.Y.Z-rcN` is marked as a **pre-release** automatically (`push.yaml`);
  - other suffixes (e.g. `-alpha1`) don't create a release at all.
- **The API version is separate.** `api_version` (`src/procs.cpp`, documented in `docs/API.md`) is an integer.
  It only changes when the script API changes incompatibly. The helper's `M.VERSION` in `lua/luabridge.lua` is
  the helper's own version.
- **Semantic versioning:**
  - a new feature means a minor bump;
  - a fix means a patch bump;
  - a breaking change to the script API means a major bump and a new `api_version`.

## Steps
1. **Prepare the release in a PR:**
   - set `"version"` in `buildspec.json`;
   - move the "Unreleased" entries in `CHANGELOG.md` under the new version;
   - update the `plugin_version` example in `docs/API.md`.

   Merge it once CI is green.
2. **Before the first public release only:** make the repository public first (see
   `docs/LAUNCH-CHECKLIST.md`). That way the release, its source and the GPL source offer are visible together.
3. **Tag `master` and push the tag** (the maintainer decides when):
   ```
   git checkout master && git pull
   git tag -a 0.9.0-beta1 -m "Lua Bridge for OBS 0.9.0-beta1"
   git push origin 0.9.0-beta1
   ```
4. **CI** (`push.yaml`) builds Release packages for Windows, macOS and Ubuntu, runs the unit tests, checks the
   tag against `buildspec.json`, and creates a **draft** GitHub release. For `0.9.0-beta1` the attachments are:
   - `obs-lua-bridge-0.9.0-beta1-windows-x64.zip`;
   - `obs-lua-bridge-0.9.0-beta1-macos-universal.pkg` and `obs-lua-bridge-0.9.0-beta1-macos-universal.tar.xz`
     (unsigned community build);
   - `obs-lua-bridge-0.9.0-beta1-x86_64-linux-gnu.deb` (+ `…-dbgsym.ddeb` debug symbols) and
     `obs-lua-bridge-0.9.0-beta1-x86_64-ubuntu-gnu.tar.xz`;
   - `obs-lua-bridge-0.9.0-beta1-source.tar.xz`: the complete source, which the GPL requires us to offer;
   - `obs-lua-bridge-lua-0.9.0-beta1.zip`: only the Lua helper and examples;
   - checksums in the release text.
5. **Check the draft:**
   - download the Windows zip and install it as `docs/INSTALL.md` says;
   - start OBS and check that the log says `version 0.9.0-beta1` and that the dock appears.
6. **Write the release text:**
   - the CHANGELOG section for this version;
   - for a beta, the beta notice and the "Mac testers wanted" call (`docs/launch/forum-resource.md` has wording);
   - the checksums CI added.
7. **Publish** the draft.

Nothing is public until step 7. To retract a draft before publishing, delete it on GitHub, and delete the tag:
`git push --delete origin <tag>`.

## Checklist before a public release
- [ ] CI green on `master`, including the unit tests on all three platforms
- [ ] `docs/TESTING.md` suites run on the release candidate; at least the websocket suite and a Windows install test
- [ ] `CHANGELOG.md` updated, and the tag equals `buildspec.json`'s version
- [ ] The minimum OBS version in `docs/INSTALL.md` is still right (`buildspec.json` → `obs-studio` version)
- [ ] For the first public release: `docs/LAUNCH-CHECKLIST.md` completed
