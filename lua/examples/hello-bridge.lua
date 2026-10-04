-- hello-bridge.lua: minimal Lua Bridge for OBS example
--
-- At load, asks the plugin for its info, registers the owner "hello" with a
-- "ping" command, and connects to the luabridge_command signal. Logs
-- "ping received" when the Tools menu item "Lua Bridge: Send test ping
-- (temporary)" is clicked. Without the plugin installed, the script logs that
-- and otherwise does nothing.

local obs = obslua

local OWNER = "hello"
local REGISTRATION = '{"display_name":"Hello Bridge","commands":[{"id":"ping","label":"Ping"}]}'

local connected = false

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

-- Calls a luabridge_* procedure with string arguments.
-- Returns ok, error, json (json is only set by luabridge_get_info).
local function call(proc, args)
	local cd = obs.calldata_create()
	for name, value in pairs(args or {}) do
		obs.calldata_set_string(cd, name, value)
	end
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), proc, cd)
	local ok = found and obs.calldata_bool(cd, "ok")
	local err = found and obs.calldata_string(cd, "error") or ("procedure " .. proc .. " not found")
	local json = obs.calldata_string(cd, "json")
	obs.calldata_destroy(cd)
	return ok, err, json
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
	return "Lua Bridge for OBS: minimal example. Logs the plugin info at load and "
		.. "\"ping received\" when you click Tools > Lua Bridge: Send test ping (temporary)."
end

function script_load(settings)
	local ok, err, info = call("luabridge_get_info")
	if not ok then
		log("Lua Bridge plugin not available (" .. tostring(err) .. "); running without it")
		return
	end

	log("info: " .. info)

	-- The signal only exists when the plugin is loaded, so connect only then
	obs.signal_handler_connect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
	connected = true

	ok, err = call("luabridge_register", { owner = OWNER, json = REGISTRATION })
	if ok then
		log("registered as '" .. OWNER .. "'")
	else
		log("registration failed: " .. tostring(err))
	end
end

function script_unload()
	if connected then
		call("luabridge_unregister", { owner = OWNER })
		obs.signal_handler_disconnect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
		connected = false
	end
end
