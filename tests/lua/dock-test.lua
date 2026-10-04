-- dock-test.lua: exercises every Lua Bridge dock control type
--
-- Load in Tools > Scripts, then open Docks > Lua Bridge. The "Dock Test"
-- section has a large label, a row of buttons (one asks for confirmation, one
-- uses label_bind), whole-number and decimal number inputs, a text input, a
-- toggle and separators. Every command the script receives is logged as
--   [dock-test.lua] command <id> origin=<origin> args=<json>
-- and shown in the "last command" label, so clicks can be checked in the log.
--
-- Start demonstrates label_bind: the button reads "Starting…", then
-- "Started ✓", then "Start" again.
--
-- The script heartbeats every 5 s through the helper; the "Last heartbeat"
-- label shows the time of the latest one (dock only, not logged). The
-- Stop/Resume heartbeat buttons are in this script's properties
-- (Tools > Scripts > select it), not in the dock, because a stale section
-- disables its dock controls. About 30 s after stopping, the section greys out
-- and offers Remove; Resume (or any later call) registers it again.

local bridge = dofile(script_path() .. "../../lua/luabridge.lua")
local obs = obslua

local OWNER = "docktest"

local REGISTRATION = {
	display_name = "Dock Test",
	commands = {
		{ id = "start", label = "Start", description = "Set the display to running" },
		{ id = "reset", label = "Reset", description = "Reset the total to 0", confirm = true },
		{ id = "add", label = "Add seconds", args = { seconds = "int" } },
		{ id = "scale", label = "Apply factor", args = { factor = "number" } },
		{ id = "greet", label = "Greet", args = { name = "string" } },
		{ id = "set_enabled", label = "Enabled", description = "Toggle the enabled flag", args = { value = "bool" } },
	},
	dock = {
		{ type = "label", bind = "display", style = "large" },
		{
			type = "row",
			items = {
				{ type = "button", command = "start", label_bind = "start_label" },
				{ type = "button", command = "reset" },
			},
		},
		{ type = "separator" },
		{ type = "number", id = "add_secs", label = "Seconds", min = 1, max = 3600, default = 5 },
		{ type = "button", command = "add", args_from = { seconds = "add_secs" } },
		{ type = "number", id = "factor", label = "Factor", min = 0, max = 10, default = 1.5 },
		{ type = "button", command = "scale", args_from = { factor = "factor" } },
		{ type = "text", id = "name", label = "Name", default = "world" },
		{ type = "button", command = "greet", args_from = { name = "name" } },
		{ type = "toggle", bind = "enabled", command = "set_enabled" },
		{ type = "separator" },
		{ type = "label", bind = "message" },
		{ type = "label", bind = "last_command" },
		{ type = "label", bind = "last_heartbeat" },
	},
}

local total = 0

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

-- One-shot timer: runs fn once after ms milliseconds
local function after(ms, fn)
	local function once()
		obs.timer_remove(once)
		fn()
	end
	obs.timer_add(once, ms)
end

local handlers = {
	start = function()
		-- label_bind demo: the Start button shows the progress
		bridge.set_state(OWNER, { start_label = "Starting\226\128\166" })
		after(1000, function()
			bridge.set_state(OWNER, { start_label = "Started \226\156\147", display = "running" })
			after(2000, function()
				bridge.set_state(OWNER, { start_label = bridge.null })
			end)
		end)
	end,
	reset = function()
		total = 0
		bridge.set_state(OWNER, { display = "0 s" })
	end,
	add = function(args)
		total = total + (args.seconds or 0)
		bridge.set_state(OWNER, { display = total .. " s" })
	end,
	scale = function(args)
		bridge.set_state(OWNER, { message = string.format("factor %.3f", args.factor or 0) })
	end,
	greet = function(args)
		bridge.set_state(OWNER, { message = "Hello, " .. tostring(args.name or "") .. "!" })
	end,
	set_enabled = function(args)
		bridge.set_state(OWNER, { enabled = args.value == true })
	end,
}

local function on_command(command, args, origin)
	local text = bridge.json.encode(args)
	log(string.format("command %s origin=%s args=%s", command, origin, text))
	local handler = handlers[command]
	if handler then
		handler(args)
	end
	bridge.set_state(OWNER, { last_command = string.format("%s from %s: %s", command, origin, text) })
end

local function on_heartbeat(ok)
	if ok then
		-- Dock only, not logged: shows heartbeats arriving and stopping
		bridge.set_state(OWNER, { last_heartbeat = "Last heartbeat: " .. os.date("%H:%M:%S") })
	end
end

function script_description()
	return "Lua Bridge for OBS: dock test. Shows every dock control type in Docks > Lua Bridge "
		.. "and logs each command. Use the buttons below to stop and resume the heartbeat."
end

function script_properties()
	local props = obs.obs_properties_create()
	obs.obs_properties_add_button(props, "stop_heartbeat", "Stop heartbeat", function()
		if bridge.set_heartbeat(OWNER, false) then
			log("heartbeat stopped; the dock section goes stale in about 30 s")
		end
		return false
	end)
	obs.obs_properties_add_button(props, "resume_heartbeat", "Resume heartbeat", function()
		-- Sends a heartbeat at once; the helper registers the owner again if the
		-- plugin had forgotten it (stale, or removed from the dock)
		if bridge.set_heartbeat(OWNER, true) then
			log("heartbeat resumed")
		end
		return false
	end)
	return props
end

function script_load(settings)
	local available, why = bridge.available()
	if not available then
		log(tostring(why) .. "; nothing to show")
		return
	end
	bridge.register(OWNER, REGISTRATION, { heartbeat_interval = 5, on_heartbeat = on_heartbeat })
	bridge.on_command(OWNER, on_command)
	bridge.set_state(OWNER, {
		display = "stopped",
		enabled = false,
		message = "",
		last_command = "",
		last_heartbeat = "Last heartbeat: none yet",
	})
	log("registered as '" .. OWNER .. "'")
end

function script_unload()
	bridge.shutdown()
end
