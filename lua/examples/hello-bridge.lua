-- hello-bridge.lua: Lua Bridge for OBS round-trip check (Milestone M0)
--
-- At load, asks the plugin for its info and connects to the luabridge_command
-- signal. Logs "ping received" when the Tools menu item
-- "Lua Bridge: Send test ping" is clicked. Without the plugin installed, the
-- script logs that and otherwise does nothing.

local obs = obslua

local OWNER = "hello"

local connected = false

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

-- Returns the info JSON string, or nil plus a reason if the plugin is absent.
local function get_info()
	local cd = obs.calldata_create()
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), "luabridge_get_info", cd)
	local ok = found and obs.calldata_bool(cd, "ok")
	local json = obs.calldata_string(cd, "json")
	local err = obs.calldata_string(cd, "error")
	obs.calldata_destroy(cd)

	if not found then
		return nil, "procedure luabridge_get_info not found"
	end
	if not ok then
		return nil, err
	end
	return json
end

local function on_command(cd)
	local owner = obs.calldata_string(cd, "owner")
	if owner ~= OWNER then
		return
	end

	local command = obs.calldata_string(cd, "command")
	local json = obs.calldata_string(cd, "json")
	local origin = obs.calldata_string(cd, "origin")

	if command == "ping" then
		log(string.format("ping received (owner=%s, origin=%s, json=%s)", owner, origin, json))
	else
		log(string.format("unknown command '%s' (origin=%s)", tostring(command), tostring(origin)))
	end
end

function script_description()
	return "Lua Bridge for OBS: M0 round-trip check. Logs the plugin info at load and "
		.. "\"ping received\" when you click Tools > Lua Bridge: Send test ping."
end

function script_load(settings)
	local info, reason = get_info()
	if not info then
		log("Lua Bridge plugin not available (" .. tostring(reason) .. "); running without it")
		return
	end

	log("info: " .. info)

	-- The signal only exists when the plugin is loaded, so connect only then
	obs.signal_handler_connect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
	connected = true
	log("connected to luabridge_command")
end

function script_unload()
	if connected then
		obs.signal_handler_disconnect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
		connected = false
	end
end
