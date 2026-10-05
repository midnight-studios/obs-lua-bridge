-- ping.lua: the asking half of the ping-pong example (load pong.lua too)
--
-- SPDX-License-Identifier: MIT
-- Copyright (c) 2026 Midnight Studios. See lua/LICENSE (MIT); the plugin itself is GPL-2.0-or-later.
--
-- Registers the owner "ping" with a dock section "Ping": a "Send ping #N"
-- button and two labels. The button sends pong.lua the command
-- "ping" {n = N}; pong answers with the custom event "ponged" {n, reply, at},
-- which ping shows in the second label.
--
-- Commands are fire-and-forget: run_command only tells us whether pong
-- accepted the ping, not pong's answer. The answer arrives later as an event,
-- matched by the ping number n. See README.md.

-- The helper lives two folders up: lua/luabridge.lua. In your own scripts,
-- copy luabridge.lua next to the script and load script_path() .. "luabridge.lua".
local bridge = dofile(script_path() .. "../../luabridge.lua")

local OWNER = "ping"
local sent = 0 -- the number of the last ping pong accepted

function script_description()
	return "Lua Bridge for OBS: ping-pong example, the asking side. Load pong.lua too."
end

-- Sends the next ping to pong and shows what happened
local function send_ping()
	local n = sent + 1
	local ok, err = bridge.run_command("pong", "ping", { n = n })

	if ok then
		sent = n
		bridge.set_state(OWNER, {
			result = "Ping #" .. n .. " sent",
			next_label = "Send ping #" .. (n + 1), -- the button's caption (label_bind)
		})
	elseif err == "owner not registered" or err == "owner is stale; register again" then
		-- pong.lua isn't loaded (or was removed). Not an error for us: show it
		-- and keep going. The number isn't used up, so the next ping that gets
		-- through is still #n. (The plugin logs this once per 10 s in the OBS
		-- log, however often you click, so the log can't flood.)
		bridge.set_state(OWNER, { result = "pong not loaded" })
	else
		bridge.set_state(OWNER, { result = "Ping failed: " .. tostring(err) })
	end
end

-- Called for every custom event from any script. We only care about pong's
-- "ponged" replies.
local function on_event(owner, event, data)
	if owner ~= "pong" or event ~= "ponged" then
		return
	end
	local at = tonumber(data.at) and os.date("%H:%M:%S", data.at) or "?"
	bridge.set_state(OWNER, {
		last_pong = string.format('Last pong: #%s "%s" at %s', tostring(data.n), tostring(data.reply), at),
	})
	-- Never send a ping from here. Answering an event with a command is how
	-- two scripts end up bouncing messages forever (see pong.lua).
end

function script_load(settings)
	bridge.register(OWNER, {
		display_name = "Ping",
		commands = {
			{ id = "send", label = "Send ping", description = "Send the next ping to pong.lua" },
		},
		dock = {
			-- label_bind: the button shows the state key "next_label" as its
			-- caption ("Send ping #4"), or the command's label until it's set
			{ type = "button", command = "send", label_bind = "next_label" },
			{ type = "label", bind = "result" },
			{ type = "label", bind = "last_pong" },
		},
	})
	bridge.on_command(OWNER, function(command)
		if command == "send" then
			send_ping()
		end
	end)
	bridge.on_event(on_event)
	bridge.set_state(OWNER, {
		next_label = "Send ping #1",
		result = "No ping sent yet",
		last_pong = "Last pong: none yet",
	})
end

function script_unload()
	bridge.shutdown() -- unregisters "ping" and disconnects our handlers
end
