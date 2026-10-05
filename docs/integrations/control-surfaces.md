# Control surfaces: sending Lua Bridge commands

Lua Bridge registers an obs-websocket **vendor** named `LuaBridge`. Any tool that can send the obs-websocket 5
request **`CallVendorRequest`** can run a script command. The request looks like this:

```json
{
  "vendorName": "LuaBridge",
  "requestType": "RunCommand",
  "requestData": { "owner": "scoreboard", "command": "home_plus", "data": {} }
}
```

`owner` and `command` come from the script's registration; `data` holds the command's declared arguments (for
example `{"seconds": 10}` for the stopwatch demo's `add`). The full reference is in [API.md](../API.md#obs-websocket-vendor-api).
Websocket commands are rate limited to 30 per second per owner.

**Before any tool can connect:** enable OBS's WebSocket server in **Tools → WebSocket Server Settings**, and
copy the port (4455) and the password into the tool.

## Summary
*Research of 2026-10-05, from the tools' own documentation; we haven't tested any of these tools yet.*

| Tool | Can send `CallVendorRequest`? | How | Confidence |
|---|---|---|---|
| Bitfocus Companion (`obs-studio` module) | **Yes** | Dedicated action **"Custom – Send Vendor Request"** | Verified in the module's documentation |
| Streamer.bot | **Yes, most likely** | **OBS Raw** sub-action (any obs-websocket request) | Raw requests are documented; vendor requests aren't mentioned explicitly. *Unverified.* |
| Touch Portal (built-in OBS integration) | **Possibly** | OBS **Custom Request** action (request type + JSON data) | The action exists; whether it accepts `CallVendorRequest` isn't documented. *Unverified.* |
| Elgato Stream Deck (official "OBS Studio" plugin) | **No action found** | — | No custom/raw/vendor request action documented. *Unverified;* see the alternatives below. |

## Bitfocus Companion
**Supported.** The OBS Studio module has an action **"Custom – Send Vendor Request"**: "Send a request
registered by an OBS plugin. Vendor name, request type, and request data are defined by that plugin."

Configure:
- **Vendor name:** `LuaBridge`
- **Request type:** `RunCommand`
- **Request data:** `{"owner": "scoreboard", "command": "home_plus"}`

There's also **"Custom – Send Command"** for any obs-websocket request. With it, use request type
`CallVendorRequest` and the full JSON from the top of this page as the data.

The response appears in the module's `custom_command_*` variables. Vendor events, including Lua Bridge's
`StateChanged` and `CustomEvent`, appear in `vendor_event_*`. That allows button feedback; this is untested.

- Source: [companion-module-obs-studio HELP.md](https://github.com/bitfocus/companion-module-obs-studio/blob/main/companion/HELP.md)
  (it recommends OBS 32.1 or newer). Older wording with a `CallVendorRequest` example:
  [companion-bundled-modules HELP.md](https://github.com/bitfocus/companion-bundled-modules/blob/main/obs-studio/companion/HELP.md).
- Companion also drives Stream Deck hardware; see [Using a Stream Deck](#using-a-stream-deck).

## Using a Stream Deck
**Confirmed route: Bitfocus Companion.**
- Companion drives Elgato Stream Deck hardware directly, without Elgato's Stream Deck app.
- Its official user guide lists the supported models: Stream Deck (15 key), Mini, XL, Mk2, Pedal, +, Neo and
  Studio.
- It says: *"We recommend connecting Stream Decks without the Elgato software."*
  Source: [Companion user guide: Elgato Stream Deck](https://companion.free/user-guide/v4.2/surfaces/elgato-streamdeck/)
  (the guide linked from [Bitfocus's Companion repository](https://github.com/bitfocus/companion)).

**Setup:**
1. **Quit Elgato's Stream Deck app,** including its tray/menu-bar icon.
   - Both programs want the same USB device. Companion's issue tracker shows conflicts when both run, e.g.
     [#1760](https://github.com/bitfocus/companion/issues/1760). The "quit it" step is our advice based on this;
     the guide itself only recommends connecting without the Elgato software.
   - If the Stream Deck doesn't appear in Companion, check that no Elgato Stream Deck process is still running,
     then use **Rescan USB** in Companion's Surfaces tab.
2. **In Companion, add the OBS Studio connection** (host, port 4455, the WebSocket password).
3. **Put a button on the Stream Deck page** with the action **"Custom – Send Vendor Request"**:
   - vendor `LuaBridge`;
   - request type `RunCommand`;
   - data such as `{"owner": "hello", "command": "ping"}`.

To keep Elgato's app for other buttons, there's a separate opt-in "Elgato Plugin" mode, in which Companion
shows its buttons inside the Elgato app. We haven't tested it; see the
[Companion guide](https://companion.free/user-guide/v4.2/surfaces/elgato-plugin/).

## Streamer.bot
**Most likely supported.** The **OBS Raw** sub-action "sends custom requests directly to the OBS WebSocket
server". It takes a request type and JSON data; the C# equivalent is
`CPH.ObsSendRaw(string requestType, string data, int connection = 0)`. `CallVendorRequest` is a standard
obs-websocket 5 request, so it should work as:
- **request type:** `CallVendorRequest`
- **data:** the JSON from the top of this page

*Unverified:* the docs don't mention vendor requests specifically, and we haven't tried it. Streamer.bot's
**OBS Raw Generator** (linked from the docs) helps build the JSON.

- Sources: [OBS Raw sub-action](https://docs.streamer.bot/api/sub-actions/obs-studio/raw),
  [OBS Raw requests in C#](https://docs.streamer.bot/examples/csharp_obsraw).

## Touch Portal
**Possibly supported.** Touch Portal's OBS integration has a **Custom Request** action with the fields
**Request Type**, **Request ID** and **Request Data** (JSON), "to use requests that are not yet available in
Touch Portal". Its guide only shows a regular request (`SetSceneItemEnabled`). If the action passes any request
type through, use:
- **Request Type:** `CallVendorRequest`
- **Request Data:** the JSON from the top of this page

*Unverified:* the guide doesn't say whether vendor requests are accepted, or which obs-websocket version it targets.

- Source: [How to use the OBS Custom Request action](https://www.touch-portal.com/blog/post/tutorials/obs-touch-portal-how-to-use-custom-request.php).

## Elgato Stream Deck
**No vendor-request action found in the official plugin.**
- Elgato's "OBS Studio" plugin for the Stream Deck app (version 2.0, which replaced the archived
  [streamdeck-obs-plugin](https://github.com/elgatosf/streamdeck-obs-plugin)) offers fixed actions: scenes,
  sources, recording, streaming and so on.
- We found no documented action for raw obs-websocket or vendor requests. Elgato's help pages couldn't be
  checked automatically (they block non-browser access), so this is *unverified*.

**BarRaider's "OBS Tools"** (a popular third-party Stream Deck plugin) lists no custom-request action either.
- Its product page still says it needs the old obs-websocket 4.9.1, while its getting-started guide refers to
  OBS's built-in WebSocket server. Treat its status as *unverified*.
- Sources: [barraider.com/obs.html](https://barraider.com/obs.html),
  [OBS Tools: Getting Started](https://docs.barraider.com/faqs/obs-tools/getting-started/).

**Options for Stream Deck users today:**
1. **Companion,** which drives Stream Deck hardware itself; see [Using a Stream Deck](#using-a-stream-deck).
   This is the confirmed route.
2. **A Stream Deck plugin that sends raw websocket messages:** the request is plain JSON over obs-websocket 5,
   but the client must also handle the obs-websocket handshake, so a generic WebSocket-message plugin isn't
   enough. *Unverified; not tested.*
3. **An OBS hotkey:**
   - a Lua script can register a hotkey with `obs_hotkey_register_frontend` and call
     `bridge.run_command(...)` when it's pressed;
   - assign it a key combination in OBS's Settings → Hotkeys;
   - Stream Deck's built-in **System → Hotkey** action presses that combination;
   - this needs no websocket at all, at the cost of one key combination per command. OBS hotkeys work while
     OBS is in the background on Windows; on macOS and Linux check your OBS hotkey settings.

## Help us verify
If you use one of these tools, try the request above against the `hello-bridge` example
(`owner: "hello"`, `command: "ping"`). Then tell us in an issue whether it worked, with the tool's version,
so this page can mark it verified.
