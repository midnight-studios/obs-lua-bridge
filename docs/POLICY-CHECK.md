# OBS Forum resource policy: how Lua Bridge meets it

The OBS Forum rejected the 0.9.0-beta1 resource under its
[resource IP policy](https://obsproject.com/forum/threads/obs-forums-resource-ip-policy.193325/). This page quotes
each rule of that policy, checks Lua Bridge against it, and says where. Re-check it before every forum post or
update.

**Policy text:** as pasted by the maintainer from the forum, "Last edited: Jul 30, 2026". The page needs a forum
login (it returns HTTP 404 without one). If the policy's "Last edited" date has changed, re-read it and update
this page.

**Status:** Met · **Not met** · **Judgment** (the moderators decide; what we did to help is listed).

## Summary

Every rule is met except two, and one of those can't be fixed with documentation:
- **AI (S5): not met.** The policy doesn't permit resources "created or written entirely, or in large part, with
  AI coding tools". By the README's own disclosure, Claude wrote most of the code. The disclosure meets the
  disclaimer requirement, but not the rule itself. Posting is the maintainer's decision; the moderators may deny
  the resource on this alone.
- **Description written with an LLM (I4): not met yet.** The forum draft was drafted by Claude. The policy asks
  authors to write and check the description themselves. The maintainer should write it before posting.

## Submission requirements

| # | Rule (quoted) | Status | How Lua Bridge meets it | Where |
|---|---|---|---|---|
| S1 | "A resource that is an OBS Studio plugin must, by definition, be compliant with the GPL license OBS uses … a plugin must have the source code available in compliance with the license. A public repository that is kept up to date is the simplest way to achieve this, and is the preferred method." | Met | The plugin is GPL-2.0-or-later, in a public repository, and every release attaches the complete source as `obs-lua-bridge-<version>-source.tar.xz` (CI: `push.yaml`). The Lua helper and examples are MIT. | `LICENSE`, `lua/LICENSE`; README → License; release template → Downloads; forum draft → Source and license |
| S2 | "Submissions should be stable releases … if something is clearly not finished, it will be rejected." | Judgment | 0.9.0-beta2 is labelled a beta. All its features are finished and tested (see AI usage); "beta" stands for missing platform coverage (no real-Mac test, unsigned macOS build), not unfinished features. The moderators may still read "beta" as not release-ready. | README banner; forum draft → Beta notice |
| S3 | "If you are hosting a download on an external link, please use a reputable host. Services like Google Drive or GitHub itself are preferable …" | Met | Downloads are GitHub release assets only. | `docs/RELEASING.md`; forum draft → Install |
| S4 | "The resource description should be well formatted, clear, and as concise as possible." | Judgment | The forum draft has short sections (what it does, install, compatibility, bug reports, AI usage, source). See I4: the maintainer should write the final text. | `docs/launch/forum-resource.md` |
| S5 | "Resources created or written entirely, or in large part, with AI coding tools are not permitted. While the use of AI or LLM tools during creation of a resource is not strictly prohibited, we require any resources that use these tools for code, art, or otherwise to provide a disclaimer in their description stating what and how these tools were used in development. Providing this disclaimer does not mean that the resource will be approved." | **Not met** (disclaimer: met) | **Disclaimer:** the README's "AI usage" section says what was AI-assisted (design, code, tests, docs), with which tool, the maintainer's role, and how it was reviewed and tested. The same text goes into every release and the forum description. **The rule itself:** most of the code was written by Claude, so the resource was written "in large part" with an AI coding tool. No documentation change fixes that. | README → AI usage; release template → AI usage; forum draft → AI usage |

## Resource branding

These rules apply to the plugin everywhere, not only on the forum.

| # | Rule (quoted) | Status | How Lua Bridge meets it | Where |
|---|---|---|---|---|
| B1 | "Do not suggest an official partnership or affiliation with OBS or the OBS Project. It should be clear to users that your app or plugin is not part of OBS or developed by the OBS Project." | Met | "Lua Bridge is an independent plugin for OBS Studio and not a product of the OBS Project." INSTALL.md also says to report problems here, not to the OBS Project. OBS's Plugin Manager description and the `.deb` description say it too. | README (top); `docs/INSTALL.md` (top); release template; forum draft (top); `src/plugin-main.cpp`; `cmake/linux/defaults.cmake` |
| B2 | "Do not use the letters "OBS" to identify the resource. Usage of the OBS acronym should be avoided in resource names whenever possible. … In certain cases, we permit the usage of the words "for OBS" but this is on a case by case basis and at our discretion." | Met | The name is **Lua Bridge**, without "OBS" or "for OBS": the README title, docs, the dock (Docks → Lua Bridge), OBS's Plugin Manager (`obs_module_name`), the locale files, the example scripts' descriptions, the package metadata (`displayName`, so release titles are "Lua Bridge <version>") and the forum title. "OBS" appears only in descriptions, such as the `.deb` summary "… for OBS Studio Lua scripts". **Kept as technical identifiers:** the file and folder names `obs-lua-bridge` (also in download file names), the procedures and signals `luabridge_*`, the websocket vendor `LuaBridge`, owner IDs. | `buildspec.json`; `src/plugin-main.cpp`; `data/locale/*.ini`; `README.md`; `lua/examples/*.lua`; `docs/launch/forum-resource.md` |
| B3 | "Do not use the OBS logo or IP without authorization … the OBS Studio logo and other associated OBS Project assets should not be used in your resource icons, marketing, or other visual assets." Screenshots are allowed "only when it is made clear that your application or plugin is a third party tool". | Met | The repository has one image, `docs/images/dock.png`: a screenshot of the dock and a text source, with no OBS logo or window icon, and B1's statement is on every page that shows it. No icons are bundled in the packages; the macOS installer has no background image; the GitHub repository has no custom social preview image. | `docs/images/`; `cmake/macos/resources/installer-macos.pkgproj.in` (no `BACKGROUND`); repository settings |
| B4 | "Usage of "obs-" in code repository names is permitted only if … The repo contains a README.md or similar file that replaces the title for the repo with one that follows the rules set forth above. Usage of "obs-" as a repository identifier is still heavily discouraged for new projects to avoid confusion." | Met | The README's title is "Lua Bridge". The repository keeps `obs-lua-bridge` because renaming it would break existing links and install folders; the policy discourages the prefix but permits it with this README. | `README.md` |

## Resource information

| # | Rule (quoted) | Status | How Lua Bridge meets it | Where |
|---|---|---|---|---|
| I1 | "Resources should include an informative description of what your resource does. We highly recommend including screenshots or videos …" | Met | The forum draft explains what it does and has a features list; post it with the dock screenshot. | forum draft → What it does; `docs/images/dock.png` |
| I2 | "Include details or link to documentation about how to use your resource … not simply an advertisement or outbound link to your other products or services." | Met | Install steps, a quick start and links to INSTALL.md and API.md. No other products or donation links. | forum draft → Install, Using a Stream Deck |
| I3 | "Include information about how users can report issues or bugs with your resource." | Met | GitHub Issues, with what to include: OBS version, OS, Lua Bridge version, the OBS log (Help → Log Files → Upload Current Log File) and steps. The issue form requires the same. Security problems go through private reporting. | README → Reporting bugs; `.github/ISSUE_TEMPLATE/bug_report.yml`; `SECURITY.md`; release template; forum draft → Reporting bugs |
| I4 | "Using LLM (i.e. ChatGPT) tools to write your description is also heavily discouraged. While these tools can be used to assist, we recommend that you take the time to write the description and validate the content yourself …" | **Not met yet** | `docs/launch/forum-resource.md` was drafted by Claude. Before posting, the maintainer writes the description in their own words, using the draft only as a checklist of what must be in it. | `docs/launch/forum-resource.md` |
| I5 | "If your resource description is non-English, please provide an English translation …" | Met | The description is in English. | — |

## Related changes in 0.9.0-beta2
- The macOS `.pkg` no longer installs debug symbols (`.dSYM`) into users' plugin folders.
- OBS 33's "Legacy" label is explained in the README and INSTALL.md (the plugin still loads; the layout will move
  once the official OBS plugin template supports the new one).

## Before posting to the forum
- [ ] The maintainer has decided to post despite S5, knowing the resource may be denied for it.
- [ ] The description is written by the maintainer (I4), and keeps B1's statement under the title, the AI usage
      section (the README's, verbatim), Reporting bugs and the source link.
- [ ] The resource's images contain no OBS logo (a cropped screenshot of the dock, like `docs/images/dock.png`).
- [ ] The release linked from the resource uses `docs/RELEASE-NOTES-TEMPLATE.md`.
- [ ] The policy's "Last edited" date is still Jul 30, 2026; if not, re-read it and update this page.
