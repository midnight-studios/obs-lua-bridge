-- dock-test.lua: exercises every Lua Bridge dock control type (M3)
--
-- Load in Tools > Scripts, then open Docks > Lua Bridge. The "Dock Test"
-- section has a large label, a row of buttons (one asks for confirmation),
-- whole-number and decimal number inputs, a text input, a toggle and
-- separators. Every command the script receives is logged as
--   [dock-test.lua] command <id> origin=<origin> args=<json>
-- and shown in the "last command" label, so clicks can be checked in the log.
--
-- The script heartbeats every 5 s; the "Last heartbeat" label shows the time of
-- the latest one (dock only, not logged). The Stop/Resume heartbeat buttons are in
-- this script's properties (Tools > Scripts > select it), not in the dock,
-- because a stale section disables its dock controls. About 30 s after
-- stopping, the section greys out and offers Remove.

local obs = obslua

local OWNER = "docktest"
local HEARTBEAT_MS = 5000
local REGISTRATION = '{"display_name":"Dock Test","commands":['
	.. '{"id":"start","label":"Start","description":"Set the display to running"},'
	.. '{"id":"reset","label":"Reset","description":"Reset the total to 0","confirm":true},'
	.. '{"id":"add","label":"Add seconds","args":{"seconds":"int"}},'
	.. '{"id":"scale","label":"Apply factor","args":{"factor":"number"}},'
	.. '{"id":"greet","label":"Greet","args":{"name":"string"}},'
	.. '{"id":"set_enabled","label":"Enabled","description":"Toggle the enabled flag","args":{"value":"bool"}}'
	.. '],"dock":['
	.. '{"type":"label","bind":"display","style":"large"},'
	.. '{"type":"row","items":[{"type":"button","command":"start"},{"type":"button","command":"reset"}]},'
	.. '{"type":"separator"},'
	.. '{"type":"number","id":"add_secs","label":"Seconds","min":1,"max":3600,"default":5},'
	.. '{"type":"button","command":"add","args_from":{"seconds":"add_secs"}},'
	.. '{"type":"number","id":"factor","label":"Factor","min":0,"max":10,"default":1.5},'
	.. '{"type":"button","command":"scale","args_from":{"factor":"factor"}},'
	.. '{"type":"text","id":"name","label":"Name","default":"world"},'
	.. '{"type":"button","command":"greet","args_from":{"name":"name"}},'
	.. '{"type":"toggle","bind":"enabled","command":"set_enabled"},'
	.. '{"type":"separator"},'
	.. '{"type":"label","bind":"message"},'
	.. '{"type":"label","bind":"last_command"},'
	.. '{"type":"label","bind":"last_heartbeat"}'
	.. "]}"

local connected = false
local heartbeat_running = false

-- Script-side state, republished whenever the owner (re-)registers
local state = {
	display = "stopped",
	total = 0,
	enabled = false,
	message = "",
	last_command = "",
	last_heartbeat = "Last heartbeat: none yet",
}

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

-- Calls a luabridge_* procedure with string arguments; returns ok, error
local function call(proc, args)
	local cd = obs.calldata_create()
	for name, value in pairs(args) do
		obs.calldata_set_string(cd, name, value)
	end
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), proc, cd)
	local ok = found and obs.calldata_bool(cd, "ok")
	local err = found and (obs.calldata_string(cd, "error") or "") or ("procedure " .. proc .. " not found")
	obs.calldata_destroy(cd)
	return ok, err
end

-- Publishes the whole state; obs_data builds (and escapes) the JSON
local function publish_state()
	local data = obs.obs_data_create()
	obs.obs_data_set_string(data, "display", state.display)
	obs.obs_data_set_int(data, "total", state.total)
	obs.obs_data_set_bool(data, "enabled", state.enabled)
	obs.obs_data_set_string(data, "message", state.message)
	obs.obs_data_set_string(data, "last_command", state.last_command)
	obs.obs_data_set_string(data, "last_heartbeat", state.last_heartbeat)
	local ok, err = call("luabridge_set_state", { owner = OWNER, json = obs.obs_data_get_json(data) })
	obs.obs_data_release(data)
	if not ok then
		obs.script_log(obs.LOG_WARNING, "set_state failed: " .. tostring(err))
	end
end

local function register()
	local ok, err = call("luabridge_register", { owner = OWNER, json = REGISTRATION })
	if ok then
		log("registered as '" .. OWNER .. "'")
		publish_state()
	else
		obs.script_log(obs.LOG_WARNING, "registration failed: " .. tostring(err))
	end
	return ok
end

local function heartbeat()
	local ok, err = call("luabridge_heartbeat", { owner = OWNER })
	if not ok then
		obs.script_log(obs.LOG_WARNING, "heartbeat failed: " .. tostring(err))
		return
	end
	-- Shown in the dock only (not logged), so heartbeats can be seen arriving and stopping
	state.last_heartbeat = "Last heartbeat: " .. os.date("%H:%M:%S")
	call("luabridge_set_state", { owner = OWNER, json = '{"last_heartbeat":"' .. state.last_heartbeat .. '"}' })
end

local function start_heartbeat()
	if not heartbeat_running then
		heartbeat()
		obs.timer_add(heartbeat, HEARTBEAT_MS)
		heartbeat_running = true
	end
end

local function stop_heartbeat()
	if heartbeat_running then
		obs.timer_remove(heartbeat)
		heartbeat_running = false
	end
end

local function on_command(cd)
	if obs.calldata_string(cd, "owner") ~= OWNER then
		return
	end
	local command = obs.calldata_string(cd, "command")
	local json = obs.calldata_string(cd, "json")
	local origin = obs.calldata_string(cd, "origin")
	log(string.format("command %s origin=%s args=%s", command, origin, json))

	local args = obs.obs_data_create_from_json(json)
	if command == "start" then
		state.display = "running"
	elseif command == "reset" then
		state.total = 0
		state.display = "0 s"
	elseif command == "add" then
		state.total = state.total + obs.obs_data_get_int(args, "seconds")
		state.display = state.total .. " s"
	elseif command == "scale" then
		state.message = string.format("factor %.3f", obs.obs_data_get_double(args, "factor"))
	elseif command == "greet" then
		state.message = "Hello, " .. obs.obs_data_get_string(args, "name") .. "!"
	elseif command == "set_enabled" then
		state.enabled = obs.obs_data_get_bool(args, "value")
	end
	obs.obs_data_release(args)

	state.last_command = string.format("%s from %s: %s", command, origin, json)
	publish_state()
end

function script_description()
	return "Lua Bridge for OBS: dock test. Shows every dock control type in Docks > Lua Bridge "
		.. "and logs each command. Use the buttons below to stop and resume the heartbeat."
end

function script_properties()
	local props = obs.obs_properties_create()
	obs.obs_properties_add_button(props, "stop_heartbeat", "Stop heartbeat", function()
		stop_heartbeat()
		log("heartbeat stopped; the dock section goes stale in about 30 s")
		return false
	end)
	obs.obs_properties_add_button(props, "resume_heartbeat", "Resume heartbeat", function()
		-- A stale (or removed) owner must register again
		if connected and register() then
			start_heartbeat()
			log("heartbeat resumed")
		end
		return false
	end)
	return props
end

function script_load(settings)
	local ok, err = call("luabridge_get_info", {})
	if not ok then
		log("Lua Bridge plugin not available (" .. tostring(err) .. "); nothing to show")
		return
	end

	obs.signal_handler_connect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
	connected = true
	if register() then
		start_heartbeat()
	end
end

function script_unload()
	stop_heartbeat()
	if connected then
		local ok, err = call("luabridge_unregister", { owner = OWNER })
		if not ok then
			obs.script_log(obs.LOG_WARNING, "unregister failed: " .. tostring(err))
		end
		obs.signal_handler_disconnect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
		connected = false
	end
end
