-- pong.lua: the answering half of the ping-pong example (load ping.lua too)
--
-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Midnight Studios. See lua/LICENSE (MIT); the plugin itself is GPL-2.0-or-later.
--
-- Registers the owner "pong" with one command, "ping" {n = <whole number>}.
-- Every ping is answered with the custom event "ponged" {n, reply = "pong", at},
-- which ping.lua (or any other script, or a websocket client) can listen for.
-- The dock section "Pong" counts the pings and shows the last reply.
--
-- Why an event and not a return value? Commands are fire-and-forget: the
-- sender only learns whether its command was accepted, never what the
-- receiver did with it. Replies travel back as events, matched by an id
-- (here the ping number n). See README.md.

-- The helper lives two folders up: lua/luabridge.lua. In your own scripts,
-- copy luabridge.lua next to the script and load script_path() .. "luabridge.lua".
local bridge = dofile(script_path() .. "../../luabridge.lua")

local OWNER = "pong"
local pongs = 0 -- how many pings this script has answered since it was loaded

function script_description()
	return "Lua Bridge for OBS: ping-pong example, the answering side. Load ping.lua too."
end

-- Called for every command sent to "pong" (from ping.lua, the dock, or websocket)
local function on_command(command, args)
	if command ~= "ping" then
		return
	end

	-- 1. Update our own counter and state (shown in our dock section)
	pongs = pongs + 1
	local at = os.time()
	bridge.set_state(OWNER, {
		count = pongs, -- the number, for websocket clients and tests
		pongs = "Pongs: " .. pongs, -- the same as text, for the dock label
		last_reply = string.format("Last reply: #%d at %s", args.n or 0, os.date("%H:%M:%S", at)),
	})

	-- 2. Reply with an event. Whoever sent ping #n matches the reply by n.
	bridge.emit(OWNER, "ponged", { n = args.n or 0, reply = "pong", at = at })

	-- 3. That's all. We never call bridge.run_command() here, not even to
	--    answer ping. If pong answered with a command and ping answered that
	--    with another ping, the two scripts would bounce commands at each
	--    other forever: the plugin delivers each one later (never inside this
	--    call), so nothing crashes, but the endless loop floods OBS's UI thread
	--    and the log. An event is a one-way notification; the listener decides
	--    what to do with it, and ping.lua never answers an event with a command.
end

function script_load(settings)
	bridge.register(OWNER, {
		display_name = "Pong",
		commands = {
			{ id = "ping", label = "Ping", description = "Answer with the event \"ponged\"", args = { n = "int" } },
		},
		-- Two labels; "bind" names the state key each one shows
		dock = {
			{ type = "label", bind = "pongs" },
			{ type = "label", bind = "last_reply" },
		},
	})
	bridge.on_command(OWNER, on_command)
	bridge.set_state(OWNER, { count = 0, pongs = "Pongs: 0", last_reply = "Last reply: none yet" })
end

function script_unload()
	bridge.shutdown() -- unregisters "pong" and disconnects our handlers
end
