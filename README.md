# Lua Bridge for OBS

An OBS Studio plugin that gives Lua scripts what OBS's scripting API lacks:
- **a dock UI:** buttons, labels, inputs and toggles for your script in **Docks → Lua Bridge**;
- **commands:** from that dock, from other scripts, and from **obs-websocket** clients such as Stream Deck or
  custom tools;
- **shared state and custom events** that websocket clients can read and receive.

Scripts keep working when the plugin isn't installed: the helper library falls back silently.

```lua
local bridge = dofile(script_path() .. "luabridge.lua")

function script_load(settings)
    bridge.register("hello", {
        display_name = "Hello Bridge",
        commands = { { id = "ping", label = "Ping" } },
        dock = { { type = "label", bind = "last_ping" }, { type = "button", command = "ping" } },
    })
    bridge.on_command("hello", function(command)
        bridge.set_state("hello", { last_ping = "Last ping: " .. os.date("%H:%M:%S") })
    end)
end

function script_unload()
    bridge.shutdown()
end
```

## Install
Download the package for Windows, macOS or Linux from
[Releases](https://github.com/midnight-studios/obs-lua-bridge/releases) and follow
[docs/INSTALL.md](docs/INSTALL.md). It requires OBS Studio 31.1 or newer. The macOS build
is currently unsigned; the install guide shows the one-time confirmation step.

## Documentation
- [docs/API.md](docs/API.md): the script API (procedures, signals, dock controls), the helper library and
  the obs-websocket vendor requests.
- [lua/](lua/): the helper `luabridge.lua` and example scripts (`hello-bridge`, `scoreboard`, `stopwatch-demo`).
- [docs/integrations/stopwatch.md](docs/integrations/stopwatch.md): integrating an existing script.
- [docs/TESTING.md](docs/TESTING.md): the test suites; [docs/RELEASING.md](docs/RELEASING.md): how releases are
  made; [CHANGELOG.md](CHANGELOG.md).

## Building
The plugin is built with the [OBS plugin template](https://github.com/obsproject/obs-plugintemplate) build
system. On Windows:
```
cmake --preset windows-x64
cmake --build --preset windows-x64
```
The presets `macos` and `ubuntu-x86_64` work the same way. See the template's
[wiki](https://github.com/obsproject/obs-plugintemplate/wiki) for the build requirements.

## License
- **The plugin** is licensed under the [GNU General Public License v2.0 or later](LICENSE).
- **The Lua helper and example scripts in [lua/](lua/)** are licensed under the [MIT License](lua/LICENSE), so
  you can copy `luabridge.lua` into scripts under any license, open or closed.

Third-party components are listed in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
