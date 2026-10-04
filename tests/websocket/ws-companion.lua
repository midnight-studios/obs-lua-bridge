-- ws-companion.lua: companion script for tests/websocket/test_vendor.py
--
-- Load in Tools > Scripts of the test OBS before running the Python tests.
-- Registers the owner "wstest" with commands the tests drive over obs-websocket:
--   set  {key, value} -> set_state {key: value, last_origin: <origin>}
--   del  {key}        -> set_state {key: null}
--   emit {event}      -> emit <event> with {"from":"wstest"}
--   echo {i, n, s, b} -> set_state {last_echo: <the command JSON exactly as received>}
--   tick              -> set_state {ticks: <count of tick commands since load>}

local obs = obslua

local OWNER = "wstest"
local REGISTRATION = '{"display_name":"WS Test Companion","commands":['
	.. '{"id":"set","args":{"key":"string","value":"string"}},'
	.. '{"id":"del","args":{"key":"string"}},'
	.. '{"id":"emit","args":{"event":"string"}},'
	.. '{"id":"echo","args":{"i":"int","n":"number","s":"string","b":"bool"}},'
	.. '{"id":"tick"}],'
	.. '"dock":[{"type":"label","bind":"about"}]}'

local connected = false
local ticks = 0

local function log(level, msg)
	obs.script_log(level, msg)
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

-- Builds a JSON object from string fields with obs_data, which escapes correctly
local function json_object(fields)
	local data = obs.obs_data_create()
	for key, value in pairs(fields) do
		obs.obs_data_set_string(data, key, value)
	end
	local text = obs.obs_data_get_json(data)
	obs.obs_data_release(data)
	return text
end

local function set_state(json)
	local ok, err = call("luabridge_set_state", { owner = OWNER, json = json })
	if not ok then
		log(obs.LOG_WARNING, "set_state failed: " .. tostring(err))
	end
end

local function on_command(cd)
	if obs.calldata_string(cd, "owner") ~= OWNER then
		return
	end
	local command = obs.calldata_string(cd, "command")
	local json = obs.calldata_string(cd, "json")
	local origin = obs.calldata_string(cd, "origin")

	local args = obs.obs_data_create_from_json(json)
	local function arg(name)
		return obs.obs_data_get_string(args, name)
	end

	if command == "set" then
		set_state(json_object({ [arg("key")] = arg("value"), last_origin = origin }))
	elseif command == "del" then
		-- obs_data can't hold null, so this one is written by hand (keys are [A-Za-z0-9_.])
		set_state('{"' .. arg("key") .. '":null}')
	elseif command == "emit" then
		local ok, err = call("luabridge_emit", { owner = OWNER, event = arg("event"), json = '{"from":"wstest"}' })
		if not ok then
			log(obs.LOG_WARNING, "emit failed: " .. tostring(err))
		end
	elseif command == "echo" then
		set_state(json_object({ last_echo = json }))
	elseif command == "tick" then
		ticks = ticks + 1
		set_state('{"ticks":' .. ticks .. "}")
	end

	obs.obs_data_release(args)
end

function script_description()
	return "Lua Bridge for OBS: companion for the websocket tests (tests/websocket/test_vendor.py). Registers owner 'wstest'."
end

function script_load(settings)
	local ok, err = call("luabridge_get_info", {})
	if not ok then
		log(obs.LOG_INFO, "Lua Bridge plugin not available (" .. tostring(err) .. "); nothing to do")
		return
	end

	obs.signal_handler_connect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
	connected = true

	ok, err = call("luabridge_register", { owner = OWNER, json = REGISTRATION })
	if ok then
		log(obs.LOG_INFO, "registered as '" .. OWNER .. "'")
		set_state('{"about":"Used by the websocket test suite"}')
	else
		log(obs.LOG_WARNING, "registration failed: " .. tostring(err))
	end
end

function script_unload()
	if connected then
		local ok, err = call("luabridge_unregister", { owner = OWNER })
		if not ok then
			log(obs.LOG_WARNING, "unregister failed: " .. tostring(err))
		end
		obs.signal_handler_disconnect(obs.obs_get_signal_handler(), "luabridge_command", on_command)
		connected = false
	end
end
