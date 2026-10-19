# Release notes template

Text for the GitHub release (`docs/RELEASING.md`, step 6), based on the 0.9.0-beta1 release. Replace `{{version}}`
and the `{{…}}` parts, drop the sections that don't apply (the beta and Mac sections for a final release), and keep
the CI-generated **Checksums** at the end. The statements under the title, **AI usage** and **Reporting bugs** are
required by the [OBS Forum resource policy](https://obsproject.com/forum/threads/obs-forums-resource-ip-policy.193325/)
(`docs/POLICY-CHECK.md`); keep them in every release.

The release title is set by CI: `Lua Bridge {{version}}` (`displayName` in `buildspec.json`).

---

**{{One line: what this release is, e.g. "Second public beta of Lua Bridge"}}**

Lua Bridge is an independent plugin for OBS Studio and not a product of the OBS Project.

Lua Bridge gives OBS Lua scripts a dock with controls, commands that other scripts and obs-websocket tools can
run, and live state and custom events. Scripts that use its helper keep working without the plugin.

**Requires OBS Studio 31.1 or newer.** On OBS 33, the Plugin Manager labels Lua Bridge "Legacy"; it still works
([FAQ](https://github.com/midnight-studios/obs-lua-bridge/blob/master/docs/INSTALL.md#faq)).
Install steps: [docs/INSTALL.md](https://github.com/midnight-studios/obs-lua-bridge/blob/master/docs/INSTALL.md)

### Downloads
- **Windows:** `obs-lua-bridge-{{version}}-windows-x64.zip`. Copy the `obs-lua-bridge` folder into `C:\ProgramData\obs-studio\plugins\`.
- **macOS (Apple Silicon and Intel):** `obs-lua-bridge-{{version}}-macos-universal.pkg`, or the `.tar.xz` for a manual install.
  > **This build is not signed or notarized by Apple.** macOS will say it can't verify the package. Click **Done**, then go to **System Settings → Privacy & Security** and click **Open Anyway**. With the `.tar.xz`, run `xattr -dr com.apple.quarantine` on the plugin instead. Details are in INSTALL.md.
- **Ubuntu 24.04:** `obs-lua-bridge-{{version}}-x86_64-linux-gnu.deb`. **Other Linux:** the `x86_64-ubuntu-gnu.tar.xz`.
- **Only the Lua helper and examples:** `obs-lua-bridge-lua-{{version}}.zip` (MIT).
- **Source:** `obs-lua-bridge-{{version}}-source.tar.xz` (GPL-2.0-or-later), or the repository.

### What's new
{{The CHANGELOG section for this version, shortened.}}

Full list: [CHANGELOG.md](https://github.com/midnight-studios/obs-lua-bridge/blob/master/CHANGELOG.md)

### AI usage
{{The README's "AI usage" section, verbatim.}}

### Mac testers wanted
I haven't been able to test on a real Mac yet. If you have one:
1. install the `.pkg`;
2. check that **Docks → Lua Bridge** appears and that the OBS log says `[lua-bridge] plugin loaded successfully (version {{version}})`;
3. load `hello-bridge.lua` and click **Ping**;
4. tell me how it went, including your macOS version and chip, in an [issue](https://github.com/midnight-studios/obs-lua-bridge/issues/new/choose).

### Reporting bugs
[Open an issue](https://github.com/midnight-studios/obs-lua-bridge/issues/new/choose) with your OBS version, OS, the plugin version from the log, and your log file (Help → Log Files → Upload Current Log File).
Security problems: please use [private vulnerability reporting](https://github.com/midnight-studios/obs-lua-bridge/security) (see SECURITY.md).

{{Checksums: added by CI}}
