# Releasing Lua Bridge for OBS

## Versions
- **The version lives in one place:** `"version"` in `buildspec.json`. It becomes the plugin version
  (`plugin_version` in `luabridge_get_info`) and the package file names.
- **Tags:**
  - `X.Y.Z` is a release;
  - `X.Y.Z-betaN` or `X.Y.Z-rcN` is a pre-release.

  In both cases `buildspec.json` holds `X.Y.Z`. The release job **fails** if the tag doesn't match.
- **The API version is separate.** `api_version` (`src/procs.cpp`, documented in `docs/API.md`) only
  changes when the script API changes incompatibly. The helper's `M.VERSION` in `lua/luabridge.lua` is the
  helper's own version.
- **Semantic versioning:**
  - a new feature means a minor bump;
  - a fix means a patch bump;
  - a breaking change to the script API means a major bump and a new `api_version`.

## Steps
1. **Prepare the release in a PR:**
   - set `"version"` in `buildspec.json`;
   - move the "Unreleased" entries in `CHANGELOG.md` under the new version, with today's date;
   - update the example in `docs/API.md` if `plugin_version` appears there.

   Merge it once CI is green.
2. **Tag `master` and push the tag:**
   ```
   git checkout master && git pull
   git tag -a 0.9.0-beta1 -m "Lua Bridge for OBS 0.9.0-beta1"
   git push origin 0.9.0-beta1
   ```
3. **CI** (`push.yaml`) builds Release packages for Windows, macOS and Ubuntu, runs the unit tests, checks the
   tag against `buildspec.json`, and creates a **draft** GitHub release with:
   - `obs-lua-bridge-<version>-windows-x64.zip`;
   - `obs-lua-bridge-<version>-macos-universal.pkg` and `.tar.xz` (unsigned community build);
   - `obs-lua-bridge-<version>-x86_64-linux-gnu.deb` (+ `.ddeb` debug symbols) and the `.tar.xz`;
   - `obs-lua-bridge-<version>-source.tar.xz`: the complete source, which the GPL requires us to offer;
   - `obs-lua-bridge-lua-<tag>.zip`: only the Lua helper and examples;
   - checksums in the release text.
4. **Check the draft:**
   - download the Windows zip and install it as `docs/INSTALL.md` says;
   - start OBS and check the log line and the dock;
   - add the changelog section to the release text.
5. **Publish** the draft. `-beta` and `-rc` tags are published as pre-releases.

Nothing is public until step 5. To retract a draft, delete it and the tag:
`git push --delete origin <tag>`.

## Checklist before a public release
- [ ] CI green on `master`, including the unit tests on all three platforms
- [ ] `docs/TESTING.md` suites run on the release candidate; at least the websocket suite and a Windows install test
- [ ] `CHANGELOG.md` updated
- [ ] The minimum OBS version in `docs/INSTALL.md` is still right (`buildspec.json` → `obs-studio` version)
