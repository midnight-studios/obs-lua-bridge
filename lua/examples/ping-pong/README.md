# Ping-pong: two scripts talking through Lua Bridge

`ping.lua` sends a command to `pong.lua`; `pong.lua` answers with an event.
Together they show the pattern for any two scripts that need to talk.

## Try it
1. In OBS, open **Tools → Scripts**, click **+** and add `ping.lua` from this
   folder. Click **+** again and add `pong.lua`. The order doesn't matter.
2. Open **Docks → Lua Bridge**. You see two sections, **Ping** and **Pong**.
3. Click **Send ping #1** in the Ping section.

What you should see:
- **Ping:**
  - "Ping #1 sent";
  - a moment later, `Last pong: #1 "pong" at 10:41:07`;
  - the button now says **Send ping #2**.
- **Pong:** "Pongs: 1" and "Last reply: #1 at 10:41:07".

## How it works
```
ping.lua                                   pong.lua
  bridge.run_command("pong", "ping", {n=1}) ──▶ on_command("ping", {n=1})
  ◀── accepted (ok) or "owner not registered"     set_state(...)
                                                  bridge.emit("pong", "ponged",
  on_event("pong", "ponged", {n=1, ...}) ◀──          {n=1, reply="pong", at=...})
```
- **Commands are fire-and-forget.** `run_command` only tells ping whether pong
  *accepted* the ping, never what pong did with it. A script can't return a
  value from a command handler, and can't read another script's state.
- **Replies come back as events.** pong emits `ponged` with the ping's number
  `n`, and ping matches the reply to its ping by that `n`. For several
  requests in flight, use an id like `n` in both directions.
- **Never answer with a command from inside a handler.** pong doesn't call
  ping back from its command handler, and ping doesn't send a ping from its
  event handler. Otherwise two scripts can bounce messages forever. Nothing
  crashes, but the loop floods OBS and its log.

## Removing and reloading
- **Only ping loaded:** clicking shows **"pong not loaded"**. Nothing breaks,
  and the ping number isn't used up. The plugin notes it in the OBS log
  (Help → Log Files) at most once every 10 seconds, however often you click.
- **pong removed or reloaded:** ping shows "pong not loaded" until pong is
  back, then continues where it left off. pong's counter starts at 0 again
  after a reload; ping's numbers keep going.
- **ping reloaded:** ping's numbers start at #1 again; pong keeps counting.
- **Closing OBS** with both loaded: both scripts unregister cleanly.

## Using the pattern in your own scripts
Copy `luabridge.lua` next to your script and load it with
`dofile(script_path() .. "luabridge.lua")` (these examples use
`"../../luabridge.lua"` because the helper is two folders up). Then:
1. send requests with `bridge.run_command(other_owner, command, {id = ...})`;
2. in the other script, answer with `bridge.emit(owner, "something_done", {id = ..., ...})`;
3. listen with `bridge.on_event(function(owner, event, data) ... end)` and match `data.id`.

Full reference: [docs/API.md](../../../docs/API.md).
