Lua Bridge for OBS: Lua helper and examples
===========================================

luabridge.lua   The helper library for your own scripts.
examples/       Ready-to-run example scripts:
                  hello-bridge.lua    the smallest example (a Ping button)
                  scoreboard.lua      a score board with +/- buttons and events
                  stopwatch-demo.lua  a stopwatch controlled from the dock

Try the examples
----------------
In OBS, open Tools > Scripts, click +, and pick a file from the examples
folder right here. The examples load ../luabridge.lua, so leave them in this
folder. Their controls appear in Docks > Lua Bridge.

Use the helper in your own script
---------------------------------
Copy luabridge.lua next to your script and load it with

    local bridge = dofile(script_path() .. "luabridge.lua")

Your script keeps working when the plugin isn't installed: every call then
returns false, "Lua Bridge plugin not available" and does nothing.

API reference: https://github.com/midnight-studios/obs-lua-bridge/blob/master/docs/API.md

License
-------
The files in this folder are MIT-licensed (see LICENSE here), so you can copy
luabridge.lua into scripts under any license. The Lua Bridge plugin itself is
GPL-2.0-or-later.
