Lua Bridge: Lua helper and examples
===================================

luabridge.lua   The helper library for your own scripts.
examples/       Ready-to-run example scripts:
                  hello-bridge.lua    the smallest example (a Ping button)
                  scoreboard.lua      a score board with +/- buttons and events
                  stopwatch-demo.lua  a stopwatch controlled from the dock
                  ping-pong/          two scripts talking to each other
                                      (load ping.lua and pong.lua; see its README.md)
stopwatch/      StopWatch 6.2, the full timer script, with its guide
                (STOPWATCH-LUABRIDGE.md) and release note. In the repository
                and as separate release downloads only: it isn't installed
                with the plugin or included in the helper zip. It works with
                or without the plugin.

Try the examples
----------------
In OBS, open Tools > Scripts, click +, and pick a file from the examples
folder right here. The examples load luabridge.lua from this folder
(../luabridge.lua, or ../../luabridge.lua for ping-pong), so leave them where
they are. Their controls appear in Docks > Lua Bridge.

Use the helper in your own script
---------------------------------
Copy luabridge.lua next to your script and load it with

    local bridge = dofile(script_path() .. "luabridge.lua")

Your script keeps working when the plugin isn't installed: every call then
returns false, "Lua Bridge plugin not available" and does nothing.

API reference: https://github.com/midnight-studios/obs-lua-bridge/blob/master/docs/API.md

License
-------
The files in this folder, StopWatch in stopwatch/ included, are MIT-licensed
(see LICENSE here), so you can copy luabridge.lua into scripts under any
license. The Lua Bridge plugin itself is
GPL-2.0-or-later.
