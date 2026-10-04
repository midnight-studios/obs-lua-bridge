-- helper-harness.lua: in-OBS checks of lua/luabridge.lua against the real plugin
--
-- Load in Tools > Scripts. Prints PASS/FAIL lines and, after 2 seconds, a final
-- RESULT line to the script log. Without the plugin it reports that the plugin
-- is not available and that every call fell back cleanly.

local bridge = dofile(script_path() .. "../../lua/luabridge.lua")
local obs = obslua

local OWNER = "helper.harness"
local passed, failed = 0, 0
local received_commands, received_events = {}, {}
local timer_active = false

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

local function check(name, condition, detail)
	if condition then
		passed = passed + 1
		log("PASS: " .. name)
	else
		failed = failed + 1
		log("FAIL: " .. name .. (detail ~= nil and (" (" .. tostring(detail) .. ")") or ""))
	end
end

-- A raw procedure call, bypassing the helper (to simulate other actors)
local function raw(proc, args)
	local cd = obs.calldata_create()
	for k, v in pairs(args) do
		obs.calldata_set_string(cd, k, v)
	end
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), proc, cd)
	local ok = found and obs.calldata_bool(cd, "ok")
	local err = found and obs.calldata_string(cd, "error") or "not found"
	obs.calldata_destroy(cd)
	return ok, err
end

local PAYLOAD = { items = { { id = 1 }, { id = 2, tags = { { v = "a" } } } }, text = "caf\195\169 \240\159\142\174", n = 2.5 }

local function check_received()
	obs.timer_remove(check_received)
	timer_active = false

	check("command delivered with decoded args", received_commands[1] == 'echo:{"msg":"hi"}:script',
		received_commands[1])
	check("command after re-registration delivered", received_commands[2] == 'echo:{"msg":"again"}:script',
		received_commands[2])
	local event = received_events[1]
	check("event delivered to on_event", event ~= nil and event.owner == OWNER and event.event == "harness.payload",
		event and (event.owner .. " " .. event.event))
	check("event JSON round trip is exact", event ~= nil and bridge.json.encode(event.data) == bridge.json.encode(PAYLOAD),
		event and bridge.json.encode(event.data))

	bridge.shutdown()
	local ok, err = raw("luabridge_set_state", { owner = OWNER, json = '{"a":1}' })
	check("shutdown unregistered the owner", not ok and err == "owner not registered", err)
	log(string.format("RESULT: %d passed, %d failed", passed, failed))
end

function script_description()
	return "Lua Bridge for OBS: in-OBS checks of the luabridge.lua helper. Results go to the script log."
end

function script_load(settings)
	log("Lua Bridge helper harness: running (helper " .. bridge.VERSION .. ")")
	local available, why = bridge.available()
	if not available then
		local r, err = bridge.register(OWNER, { display_name = "x" })
		check("plugin absent: calls fall back cleanly", r == false and err == why, err)
		log(tostring(why))
		log(string.format("RESULT: %d passed, %d failed", passed, failed))
		return
	end

	local info = bridge.info()
	check("compatible plugin", info ~= nil and info.api_version >= bridge.API_VERSION, info and info.api_version)
	check("capabilities is an object of flags", bridge.has("run_command") and bridge.has("nonexistent") == false)

	local ok, err = bridge.register(OWNER, {
		display_name = "Helper Harness",
		commands = { { id = "echo", args = { msg = "string" } } },
		dock = { { type = "label", bind = "status" } },
	}, { heartbeat = false })
	check("register", ok, err)
	check("on_command", bridge.on_command(OWNER, function(command, args, origin)
		received_commands[#received_commands + 1] = command .. ":" .. bridge.json.encode(args) .. ":" .. origin
	end))
	check("on_event", bridge.on_event(function(owner, event, data)
		if owner == OWNER then
			received_events[#received_events + 1] = { owner = owner, event = event, data = data }
		end
	end))
	check("set_state", bridge.set_state(OWNER, { status = "ready", count = 1 }))
	check("run_command", bridge.run_command(OWNER, "echo", { msg = "hi" }))
	check("emit with nested JSON", bridge.emit(OWNER, "harness.payload", PAYLOAD))

	ok, err = bridge.emit(OWNER, "harness.bad", { tags = { "a", "b" } })
	check("plugin errors are returned (arrays of strings rejected)", not ok and
		tostring(err):find("arrays of non-objects", 1, true) ~= nil, err)

	-- Simulate the dock's Remove button: the plugin forgets the owner
	raw("luabridge_unregister", { owner = OWNER })
	ok, err = bridge.set_state(OWNER, { count = 2 })
	check("set_state after an outside unregister re-registers and succeeds", ok, err)
	local raw_ok, raw_err = raw("luabridge_set_state", { owner = OWNER, json = "{}" })
	check("owner is registered again", raw_ok, raw_err)
	check("run_command after re-registration", bridge.run_command(OWNER, "echo", { msg = "again" }))

	log("checking delivered signals in 2 seconds...")
	obs.timer_add(check_received, 2000)
	timer_active = true
end

function script_unload()
	if timer_active then
		obs.timer_remove(check_received)
		timer_active = false
	end
	bridge.shutdown()
end
