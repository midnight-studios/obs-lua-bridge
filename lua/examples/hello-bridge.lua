-- hello-bridge.lua: the smallest Lua Bridge for OBS example
--
-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Midnight Studios. See lua/LICENSE (MIT); the plugin itself is GPL-2.0-or-later.
--
-- Registers the owner "hello" with a "ping" command and a small dock section
-- (Docks > Lua Bridge): a Ping button and the time of the last ping. Clicking
-- Ping logs "ping received" and updates the label. Without the plugin the
-- script logs that once and otherwise does nothing.
--
-- It uses the helper library lua/luabridge.lua. In your own scripts, copy
-- luabridge.lua next to the script and load it with
--     dofile(script_path() .. "luabridge.lua")

local bridge = dofile(script_path() .. "../luabridge.lua")

local OWNER = "hello"

local function log(msg)
	obslua.script_log(obslua.LOG_INFO, msg)
end

function script_description()
	return "Lua Bridge for OBS: minimal example. Click Ping in the Lua Bridge dock (Docks > Lua Bridge)."
end

function script_load(settings)
	local available, why = bridge.available()
	if not available then
		log(tostring(why) .. "; running without it")
		return
	end
	log("info: " .. bridge.json.encode(bridge.info()))
	log("obs-websocket integration: " .. (bridge.has("websocket") and "available" or "not available"))

	bridge.register(OWNER, {
		display_name = "Hello Bridge",
		commands = { { id = "ping", label = "Ping", description = "Send a ping to hello-bridge.lua" } },
		dock = {
			{ type = "label", bind = "last_ping" },
			{ type = "button", command = "ping" },
		},
	})
	bridge.on_command(OWNER, function(command, args, origin)
		if command == "ping" then
			log(string.format("ping received (owner=%s, origin=%s, json=%s)", OWNER, origin, bridge.json.encode(args)))
			bridge.set_state(OWNER, { last_ping = "Last ping: " .. os.date("%H:%M:%S") })
		end
	end)
	bridge.set_state(OWNER, { last_ping = "Last ping: never" })
	log("registered as '" .. OWNER .. "'")
end

function script_unload()
	bridge.shutdown()
end
