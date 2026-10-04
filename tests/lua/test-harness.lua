-- test-harness.lua: in-OBS smoke test for Lua Bridge for OBS
--
-- Load in Tools > Scripts. Runs synchronous checks at load, then checks the
-- signals it received 2 seconds later and prints a final RESULT line to the
-- script log. Calls that are expected to fail are announced with an
-- "EXPECTED-WARN" line, because the plugin logs a warning for each of them.

local obs = obslua

local OWNER = "harness"
local CYCLES = 100
local REGISTRATION = '{"display_name":"Test Harness",'
	.. '"commands":[{"id":"ping"},{"id":"add","args":{"seconds":"int"}}],'
	.. '"dock":[{"type":"label","bind":"display"},{"type":"button","command":"ping"}]}'

local passed = 0
local failed = 0
local connected = false
local timer_active = false

-- Signals received for OWNER, in arrival order
local commands = {}
local events = {}

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

local function check(name, condition, detail)
	if condition then
		passed = passed + 1
		log("PASS: " .. name)
	else
		failed = failed + 1
		log("FAIL: " .. name .. (detail and (" (" .. tostring(detail) .. ")") or ""))
	end
end

-- Calls a luabridge_* procedure with string arguments; returns ok, error, json
local function call(proc, args)
	local cd = obs.calldata_create()
	for name, value in pairs(args or {}) do
		obs.calldata_set_string(cd, name, value)
	end
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), proc, cd)
	local ok = found and obs.calldata_bool(cd, "ok")
	local err = found and (obs.calldata_string(cd, "error") or "") or ("procedure " .. proc .. " not found")
	local json = obs.calldata_string(cd, "json")
	obs.calldata_destroy(cd)
	return ok, err, json
end

local function expect_ok(name, proc, args)
	local ok, err = call(proc, args)
	check(name, ok, err)
	return ok
end

local function expect_fail(name, proc, args, substring)
	log("EXPECTED-WARN: the next call should fail with '" .. substring .. "'")
	local ok, err = call(proc, args)
	check(name, not ok and err:find(substring, 1, true) ~= nil, ok and "succeeded" or err)
end

local function on_command(cd)
	if obs.calldata_string(cd, "owner") ~= OWNER then
		return
	end
	table.insert(commands, {
		command = obs.calldata_string(cd, "command"),
		json = obs.calldata_string(cd, "json"),
		origin = obs.calldata_string(cd, "origin"),
	})
end

local function on_event(cd)
	if obs.calldata_string(cd, "owner") ~= OWNER then
		return
	end
	table.insert(events, { event = obs.calldata_string(cd, "event"), json = obs.calldata_string(cd, "json") })
end

local function run_sync_tests()
	-- Info
	local ok, err, info = call("luabridge_get_info")
	check("get_info returns ok", ok, err)
	check("get_info reports api_version 1", ok and info:find('"api_version":1', 1, true) ~= nil, info)
	check("get_info lists run_command", ok and info:find('"run_command"', 1, true) ~= nil, info)

	-- Happy path
	expect_ok("register", "luabridge_register", { owner = OWNER, json = REGISTRATION })
	expect_ok("set_state", "luabridge_set_state", { owner = OWNER, json = '{"display":"00:00","running":false}' })
	expect_ok("heartbeat", "luabridge_heartbeat", { owner = OWNER })
	expect_ok("run_command ping", "luabridge_run_command", { owner = OWNER, command = "ping", json = "{}" })
	check("signals are queued, not delivered during the call", #commands == 0, #commands .. " already received")
	expect_ok("run_command add with int arg", "luabridge_run_command",
		{ owner = OWNER, command = "add", json = '{"seconds":5}' })
	expect_ok("emit", "luabridge_emit", { owner = OWNER, event = "harness.test", json = '{"n":1}' })

	-- Validation failures (each one logs a plugin warning)
	expect_fail("rejects invalid owner id", "luabridge_register",
		{ owner = "Bad Owner", json = REGISTRATION }, "invalid owner id")
	expect_fail("rejects invalid JSON", "luabridge_set_state", { owner = OWNER, json = "{broken" }, "invalid JSON")
	expect_fail("rejects nested state values", "luabridge_set_state",
		{ owner = OWNER, json = '{"a":{"b":1}}' }, "must be a string, number, boolean or null")
	expect_fail("rejects oversized JSON", "luabridge_set_state",
		{ owner = OWNER, json = '{"a":"' .. string.rep("x", 70000) .. '"}' }, "json exceeds 65536 bytes")
	expect_fail("rejects unknown command", "luabridge_run_command",
		{ owner = OWNER, command = "nope", json = "{}" }, "unknown command 'nope'")
	expect_fail("rejects wrong arg type", "luabridge_run_command",
		{ owner = OWNER, command = "add", json = '{"seconds":1.5}' }, "must be int")
	expect_fail("rejects unregistered owner", "luabridge_run_command",
		{ owner = "nobody", command = "ping", json = "{}" }, "owner not registered")
	expect_fail("rejects invalid event name", "luabridge_emit",
		{ owner = OWNER, event = "bad event", json = "{}" }, "invalid event name")
	expect_fail("rejects missing owner", "luabridge_register", { json = REGISTRATION }, "missing owner")

	local many = {}
	for i = 1, 65 do
		many[i] = '{"id":"c' .. i .. '"}'
	end
	expect_fail("rejects 65 commands", "luabridge_register",
		{ owner = OWNER, json = '{"display_name":"x","commands":[' .. table.concat(many, ",") .. "]}" },
		"too many commands (max 64)")

	-- Unknown dock controls are skipped with a warning; registration still succeeds
	log("EXPECTED-WARN: the next call logs a warning about an unknown dock control")
	ok, err = call("luabridge_register", {
		owner = OWNER .. ".dock",
		json = '{"display_name":"Dock test","dock":[{"type":"slider"},{"type":"separator"}]}',
	})
	check("unknown dock control is skipped, registration ok", ok, err)
	call("luabridge_unregister", { owner = OWNER .. ".dock" })

	-- Unregister then use
	expect_ok("unregister", "luabridge_unregister", { owner = OWNER })
	expect_ok("unregister again is ok (idempotent)", "luabridge_unregister", { owner = OWNER })
	expect_fail("set_state after unregister fails", "luabridge_set_state",
		{ owner = OWNER, json = '{"a":1}' }, "owner not registered")

	-- Register / set state / command / unregister cycle
	local cycle_errors = 0
	local first_error
	for i = 1, CYCLES do
		for _, step in ipairs({
			{ "luabridge_register", { owner = OWNER, json = REGISTRATION } },
			{ "luabridge_set_state", { owner = OWNER, json = '{"i":' .. i .. "}" } },
			{ "luabridge_run_command", { owner = OWNER, command = "ping", json = "{}" } },
			{ "luabridge_unregister", { owner = OWNER } },
		}) do
			local step_ok, step_err = call(step[1], step[2])
			if not step_ok then
				cycle_errors = cycle_errors + 1
				first_error = first_error or (step[1] .. ": " .. tostring(step_err))
			end
		end
	end
	check(CYCLES .. "x register/set_state/run_command/unregister all ok", cycle_errors == 0,
		cycle_errors .. " errors, first: " .. tostring(first_error))

	-- Leave the owner registered for the rest of the session
	expect_ok("re-register after cycles", "luabridge_register", { owner = OWNER, json = REGISTRATION })
end

local function check_signals()
	obs.timer_remove(check_signals)
	timer_active = false

	-- Expected order: ping, add, then one ping per cycle
	local expected = 2 + CYCLES
	check("received " .. expected .. " commands", #commands == expected, #commands .. " received")

	local in_order = #commands >= 2 and commands[1].command == "ping" and commands[2].command == "add"
	local all_script = true
	for i, c in ipairs(commands) do
		if c.origin ~= "script" then
			all_script = false
		end
		if i > 2 and c.command ~= "ping" then
			in_order = false
		end
	end
	check("commands arrived in call order", in_order)
	check("all commands have origin=script", all_script)
	check("add command carried its JSON", commands[2] ~= nil and commands[2].json == '{"seconds":5}',
		commands[2] and commands[2].json)

	check("received 1 event", #events == 1, #events .. " received")
	check("event name and JSON", events[1] ~= nil and events[1].event == "harness.test" and events[1].json == '{"n":1}',
		events[1] and (events[1].event .. " " .. events[1].json))

	log(string.format("RESULT: %d passed, %d failed", passed, failed))
end

function script_description()
	return "Lua Bridge for OBS: in-OBS smoke test. Results (PASS/FAIL and a final RESULT line) go to the script log."
end

function script_load(settings)
	local ok, err = call("luabridge_get_info")
	if not ok then
		log("Lua Bridge plugin not available (" .. tostring(err) .. "); nothing to test")
		return
	end

	local sh = obs.obs_get_signal_handler()
	obs.signal_handler_connect(sh, "luabridge_command", on_command)
	obs.signal_handler_connect(sh, "luabridge_event", on_event)
	connected = true

	log("Lua Bridge test harness: running")
	run_sync_tests()
	log("checking received signals in 2 seconds...")
	obs.timer_add(check_signals, 2000)
	timer_active = true
end

function script_unload()
	if timer_active then
		obs.timer_remove(check_signals)
		timer_active = false
	end
	if connected then
		call("luabridge_unregister", { owner = OWNER })
		local sh = obs.obs_get_signal_handler()
		obs.signal_handler_disconnect(sh, "luabridge_command", on_command)
		obs.signal_handler_disconnect(sh, "luabridge_event", on_event)
		connected = false
	end
end
