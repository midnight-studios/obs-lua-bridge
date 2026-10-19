# OBS Forum resource policy: how Lua Bridge meets it

The OBS Forum rejected the 0.9.0-beta1 resource under its
[resource IP policy](https://obsproject.com/forum/threads/obs-forums-resource-ip-policy.193325/). This page lists
each requirement, how this repository meets it, and where. Re-check it before every forum post or update.

The requirements below are as summarised by the maintainer for 0.9.0-beta2. The policy page itself wasn't
readable without a forum login (HTTP 404) when this page was written, so compare it with the current policy text
before posting.

| # | Requirement | How Lua Bridge meets it | Where |
|---|---|---|---|
| 1 | **GPL source available.** | The plugin is GPL-2.0-or-later. The full source is public on GitHub, and every release attaches the complete source as `obs-lua-bridge-<version>-source.tar.xz` (CI: `push.yaml`). The Lua helper and examples are MIT. | `LICENSE`, `lua/LICENSE`; README → License; `docs/RELEASE-NOTES-TEMPLATE.md` → Downloads; forum draft → Source and license |
| 2 | **AI disclosure:** what was AI-assisted, and how. | A section saying which parts were AI-assisted (design, code, tests, docs), with which tool, and how the results were reviewed and tested. The same text goes into every release and the forum resource. | README → AI usage; `docs/RELEASE-NOTES-TEMPLATE.md` → AI usage; forum draft → AI usage |
| 3 | **Not an OBS Project product.** | "Lua Bridge is an independent plugin for OBS Studio and not a product of the OBS Project." INSTALL.md also says where to report problems (here, not to the OBS Project). The `.deb` description says it too. | README (top); `docs/INSTALL.md` (top); `docs/RELEASE-NOTES-TEMPLATE.md`; forum draft (top); `cmake/linux/defaults.cmake` |
| 4 | **Naming:** "for OBS" is decided case by case; an `obs-` repository prefix is fine with a compliant README title. | The user-visible name is **Lua Bridge**: the README title, docs, the dock (Docks → Lua Bridge), OBS's Plugin Manager (`obs_module_name`), the locale files, the example scripts' descriptions, the package metadata (`displayName`, so release titles are "Lua Bridge <version>") and the forum title. Kept for compatibility: the repository and file names `obs-lua-bridge`, the procedures and signals `luabridge_*`, the websocket vendor `LuaBridge`, owner IDs. | `buildspec.json`; `src/plugin-main.cpp`; `data/locale/*.ini`; `README.md`; `lua/examples/*.lua`; `docs/launch/forum-resource.md` |
| 5 | **No OBS logos.** | The repository contains one image, `docs/images/dock.png`: a screenshot of the dock and a text source, with no OBS logo or window icon. No icons are bundled in the packages; the macOS installer has no background image. The GitHub repository has no custom social preview image. | `docs/images/`; `cmake/macos/resources/installer-macos.pkgproj.in` (no `BACKGROUND` image); repository settings |
| 6 | **Bug-report instructions.** | The README explains how to report a bug on GitHub Issues and what to include: OBS version, OS, Lua Bridge version, the OBS log file (Help → Log Files → Upload Current Log File) and steps. The issue form requires the same fields. Security problems go through private reporting. | README → Reporting bugs; `.github/ISSUE_TEMPLATE/bug_report.yml`; `SECURITY.md`; `docs/RELEASE-NOTES-TEMPLATE.md`; forum draft → Reporting bugs |

## Related changes in 0.9.0-beta2
- The macOS `.pkg` no longer installs debug symbols (`.dSYM`) into users' plugin folders.
- OBS 33's "Legacy" label is explained in the README and INSTALL.md (the plugin still loads; the layout will move
  once the official OBS plugin template supports the new one).

## Before posting to the forum
- [ ] The forum text is `docs/launch/forum-resource.md`, with the statement under the title, AI usage,
      Reporting bugs and the source link intact.
- [ ] The resource's images contain no OBS logo (a cropped screenshot of the dock, like `docs/images/dock.png`).
- [ ] The release linked from the resource uses `docs/RELEASE-NOTES-TEMPLATE.md`.
- [ ] Re-read the current policy text and update this page if it changed.
