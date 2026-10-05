# Installing Lua Bridge for OBS

Requires **OBS Studio 31.1 or newer**. Download the package for your system from
the [Releases](https://github.com/midnight-studios/obs-lua-bridge/releases) page.

Every package includes the plugin, the Lua helper `luabridge.lua` and the
example scripts, in a `lua/` folder:

```
lua/luabridge.lua            the helper: copy it next to your own script
lua/examples/*.lua           examples: add them in Tools > Scripts straight from here
lua/README.txt, lua/LICENSE
```

Scripts that use the helper keep working when the plugin isn't installed, so
uninstalling never breaks them.

**Only need the helper?** Each release also has `obs-lua-bridge-lua-<version>.zip`
with just the `lua/` folder.

## Windows
1. Close OBS.
2. Download `obs-lua-bridge-<version>-windows-x64.zip` and open it.
3. Copy the `obs-lua-bridge` folder into `C:\ProgramData\obs-studio\plugins\`.
   Create the `plugins` folder if it doesn't exist; Windows may ask for
   administrator permission. You should end up with
   `C:\ProgramData\obs-studio\plugins\obs-lua-bridge\bin\64bit\obs-lua-bridge.dll`.
4. Start OBS. **Docks → Lua Bridge** is now available.

The `lua/` folder is at `C:\ProgramData\obs-studio\plugins\obs-lua-bridge\data\lua\`.

**Uninstall:** close OBS and delete `C:\ProgramData\obs-studio\plugins\obs-lua-bridge`.

## macOS
The macOS build is a **community build: it isn't signed or notarized by Apple**,
so macOS asks you to confirm it once.

**With the installer (`.pkg`):**
1. Close OBS.
2. Download `obs-lua-bridge-<version>-macos-universal.pkg` and double-click it.
3. macOS says it "cannot verify" the package. Click **Done** (not "Move to Trash").
4. Open **System Settings → Privacy & Security**, scroll down to the message about
   `obs-lua-bridge-…pkg`, and click **Open Anyway**. Confirm with your password,
   then follow the installer. It installs for your user only, into
   `~/Library/Application Support/obs-studio/plugins/`, which is where OBS looks
   for plugins.
5. Start OBS.

**Without the installer (`.tar.xz`):**
1. Close OBS and unpack `obs-lua-bridge-<version>-macos-universal.tar.xz`.
2. Move `obs-lua-bridge.plugin` to `~/Library/Application Support/obs-studio/plugins/`.
3. In Terminal, remove the download quarantine flag:
   ```
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/obs-lua-bridge.plugin
   ```
4. Start OBS.

The `lua/` folder is inside the plugin bundle, at
`~/Library/Application Support/obs-studio/plugins/obs-lua-bridge.plugin/Contents/Resources/lua/`.
In the Scripts dialog's file picker, press **Cmd+Shift+G** and paste that path. In
Finder, right-click the plugin → **Show Package Contents**. Or use the Lua-only zip
instead.

**Uninstall:** close OBS and delete `obs-lua-bridge.plugin` from
`~/Library/Application Support/obs-studio/plugins/`. The `.pkg` and the manual
install both put it there.

## Linux
**Ubuntu 24.04 (x86_64), `.deb`:**
```
sudo apt install ./obs-lua-bridge-<version>-x86_64-linux-gnu.deb
```
The `lua/` folder is at `/usr/share/obs/obs-plugins/obs-lua-bridge/lua/`.
**Uninstall:** `sudo apt remove obs-lua-bridge`.

**Other distributions, `.tar.xz`:** the archive contains `lib/` and `share/` for
a system-wide install under `/usr` (or `/usr/local`, depending on how your OBS was
built):
```
sudo tar -xJf obs-lua-bridge-<version>-x86_64-ubuntu-gnu.tar.xz -C /usr
```
**Uninstall:** delete `lib/x86_64-linux-gnu/obs-plugins/obs-lua-bridge.so` (or
`lib/obs-plugins/...`) and `share/obs/obs-plugins/obs-lua-bridge/` from the
same prefix.

**Flatpak OBS:** the Flatpak runtime may not be compatible with these builds. If
you try one, its files go into `~/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs-lua-bridge/`,
with `bin/64bit/obs-lua-bridge.so` and `data/` (the contents of `share/obs/obs-plugins/obs-lua-bridge/`).

## Checking that it works
Start OBS and open **Docks → Lua Bridge**. Without scripts it says no scripts
are registered. Add `lua/examples/hello-bridge.lua` in **Tools → Scripts**: a
"Hello Bridge" section with a **Ping** button appears. The OBS log (**Help → Log
Files**) contains `[lua-bridge] plugin loaded successfully`.
