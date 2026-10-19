<!-- DRAFT: not posted. Text for the OBS forum resource page. Replace the {{placeholders}} at launch.
     The OBS Forum resource policy (docs/POLICY-CHECK.md) requires the statement under the title, the AI usage
     section, the bug-report steps and the source link; keep them. No OBS logos in the resource's images.
     Policy rule I4: the maintainer writes the posted description in their own words; this draft is a checklist. -->

# Lua Bridge: a dock, commands and events for your Lua scripts

Lua Bridge is an independent plugin for OBS Studio and not a product of the OBS Project.

**Beta: 0.9.0-beta2.** Lua Bridge works, has been tested hard (see below), and its script API is meant to stay
stable. It's a beta because it needs more people, platforms and real scripts. Please report anything odd on
{{GitHub issues link}}.

## What it does
OBS Lua scripts can't add their own controls to OBS's main window, and they can't easily talk to each other or
to tools like Stream Deck. Lua Bridge adds that:
- **A dock for your scripts:** **Docks → Lua Bridge** shows a section for each script with buttons, labels,
  number and text inputs, toggles and separators. Buttons can ask for confirmation and can show live captions
  ("Start" / "Pause").
- **Commands:** scripts declare commands. They can be run from the dock, from other scripts, or from any
  obs-websocket 5 tool (vendor `LuaBridge`).
- **State and events:** scripts publish state (shown in the dock and readable over websocket) and send custom
  events that other scripts and websocket clients receive.
- **Safe without the plugin:** scripts that use the small helper library `luabridge.lua` keep working when Lua
  Bridge isn't installed; they just run without the dock.

## Features at a glance
- One shared dock, with one section per script. Sections update live, and survive script reloads and scene
  collection switches.
- **The Lua helper** (`luabridge.lua`, MIT-licensed): JSON, heartbeats, and automatic re-registration. Copy it
  next to your script.
- **Examples included:**
  - **hello-bridge**: the smallest example;
  - **scoreboard**: a score board with +/- buttons;
  - **stopwatch-demo**;
  - **ping-pong**: two scripts talking to each other.
- **obs-websocket requests:** `GetInfo`, `ListOwners`, `ListCommands`, `GetState`, `RunCommand`, with
  rate limiting, plus state-change and custom events.
- **Tested** before the first beta with fuzzing, a 1-hour stress test (1,000 commands per minute), an 8-hour
  soak, shutdowns with commands in flight, and scene collection and profile switching (see AI usage).

## Install
Download from {{GitHub release link}}:
- **Windows:** copy the `obs-lua-bridge` folder from the zip into `C:\ProgramData\obs-studio\plugins\`.
- **macOS:** run the `.pkg`. The beta isn't signed by Apple, so approve it once in System Settings → Privacy &
  Security → **Open Anyway**.
- **Linux:** the `.deb` for Ubuntu 24.04, or the `.tar.xz`.

Full instructions: {{docs/INSTALL.md link}}. Then try `lua/examples/hello-bridge.lua` in Tools → Scripts.

## Compatibility
- **OBS Studio 31.1 or newer.** Tested on OBS 32.2.2 (Windows 11).
- **Windows 10/11 x64:** tested.
- **macOS 12+ (universal build for Apple Silicon and Intel):** builds in CI, but **not yet tested on a real Mac**.
  See below.
- **Ubuntu 24.04 x86_64:** builds in CI; package contents checked. Other distributions can use the tarball.
- **Control surfaces:**
  - **confirmed:** Bitfocus Companion, through its "Custom – Send Vendor Request" action;
  - Streamer.bot, Touch Portal and Elgato's own Stream Deck plugin are unverified so far;
  - details: {{docs/integrations/control-surfaces.md link}}.

## Using a Stream Deck
Use **Bitfocus Companion**. It drives Stream Deck hardware directly, and its OBS module sends Lua Bridge
commands.
- **Quit Elgato's Stream Deck app first.** Companion's guide recommends connecting Stream Decks without the
  Elgato software: https://companion.free/user-guide/v4.2/surfaces/elgato-streamdeck/
- **Setup steps:** {{control-surfaces.md#using-a-stream-deck link}}.

## Beta notice
- **This is a beta.** The script API (`api_version` 1) is intended to stay compatible, with
  changes in 1.x additive only, but small behaviour changes may still happen before 1.0.
- **The macOS build isn't signed or notarized** during the beta.
- **Please report problems** on {{GitHub issues link}}, not to the OBS Project.

## AI usage
Lua Bridge was developed with extensive help from an AI coding assistant, Anthropic's Claude (through Claude
Code). Every change by a person since the repository was created from the OBS plugin template was made with it;
those commits carry a `Co-Authored-By: Claude` line.
- **Design:** the specification and the script API were worked out with Claude. The maintainer set the goals and
  made the design decisions.
- **Code:** Claude wrote most of the plugin (C++), the Lua helper and examples, and the build, packaging and CI
  scripts, following the maintainer's instructions.
- **Tests:** Claude wrote the unit tests, websocket test suites, fuzzers and stress tests, and ran them against
  test copies of OBS.
- **Docs:** Claude drafted most of the documentation; the maintainer edited it.

**The maintainer's role:** the maintainer defined the goals, made every design decision, reviewed every change
report and pull request before merging, and tested the plugin in OBS on Windows.

**Review and testing:** CI builds on Windows, macOS and Ubuntu and runs the unit tests on every pull request, and
changes merge only when CI passes. Before the first beta (0.9.0-beta1), the plugin was also tested on Windows
with fuzzing, a 1-hour stress test, an 8-hour soak, shutdowns with commands in flight, and scene collection and
profile switching; those runs haven't been repeated for 0.9.0-beta2. It hasn't been tested on a real Mac yet.

## Reporting bugs
Open an issue on {{GitHub issues link}} and include:
- your OBS version (Help → About) and operating system;
- the Lua Bridge version, from the log line `[lua-bridge] plugin loaded successfully (version …)`;
- your OBS log: Help → Log Files → Upload Current Log File, and paste the link;
- what you did, what happened, and which scripts were loaded.

## Mac testers wanted
We don't have a Mac to test on. If you do, please:
1. install the `.pkg` (with the Open Anyway step) and start OBS;
2. check that **Docks → Lua Bridge** appears and that the log says `[lua-bridge] plugin loaded successfully (version 0.9.0-beta2)`;
3. load `hello-bridge.lua` and click **Ping**;
4. tell us how it went, including your macOS version and chip (Apple Silicon or Intel), in {{issue or forum thread link}}.

Even a "works for me" helps.

## Credits
- **Lua Bridge:** Midnight Studios.
- **Thanks to Exeldro** for the original idea.
- **Built with** the OBS plugin template, obs-websocket's vendor API and nlohmann/json.

## Source and license
- **Plugin:** open source (GPL-2.0-or-later). The Lua helper and examples are MIT.
- **Source, documentation and issues:** {{GitHub repository link}}. Every release also attaches the complete
  source (`obs-lua-bridge-<version>-source.tar.xz`).
