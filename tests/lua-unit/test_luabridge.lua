-- test_luabridge.lua: unit tests for lua/luabridge.lua (run by test_luabridge.py)
--
-- Each test gets a fresh Lua state with the fake obslua (fake_obslua.lua) and a
-- freshly loaded helper: test(fake, load_helper).

local tests = {}

local function fail(message, level)
	error(message, (level or 1) + 1)
end

local function eq(actual, expected, what)
	if actual ~= expected then
		fail(string.format("%s: expected %s, got %s", what or "value", tostring(expected), tostring(actual)), 2)
	end
end

local function truthy(value, what)
	if not value then
		fail((what or "condition") .. " is false", 2)
	end
end

local function contains(text, part, what)
	if type(text) ~= "string" or not text:find(part, 1, true) then
		fail(string.format("%s: %q does not contain %q", what or "text", tostring(text), part), 2)
	end
end

local function count(t)
	local n = 0
	for _ in pairs(t) do
		n = n + 1
	end
	return n
end

---------------------------------------------------------------------------
-- JSON
---------------------------------------------------------------------------

function tests.json_encodes_scalars(_, load)
	local bridge = load()
	local j = bridge.json
	eq(j.encode(nil), "null")
	eq(j.encode(bridge.null), "null")
	eq(j.encode(true), "true")
	eq(j.encode(false), "false")
	eq(j.encode(42), "42")
	eq(j.encode(-7), "-7")
	eq(j.encode(0), "0")
	eq(j.encode(0.1), "0.1")
	eq(j.encode(2.5), "2.5")
	eq(j.encode(1234567890123), "1234567890123")
	eq(tonumber(j.encode(1e300)), 1e300, "1e300 round trip")
	eq(tonumber(j.encode(1 / 3)), 1 / 3, "1/3 round trip")
	local ok, err = j.encode(0 / 0)
	eq(ok, nil, "NaN")
	contains(err, "NaN")
	eq(j.encode(math.huge), nil, "infinity")
end

function tests.json_encodes_strings(_, load)
	local j = load().json
	eq(j.encode('say "hi"\\'), '"say \\"hi\\"\\\\"')
	eq(j.encode("a\nb\tc\r"), '"a\\nb\\tc\\r"')
	eq(j.encode("\1"), '"\\u0001"')
	eq(j.encode("caf\195\169 \240\159\142\174"), '"caf\195\169 \240\159\142\174"', "UTF-8 passes through")
end

function tests.json_encodes_tables(_, load)
	local bridge = load()
	local j = bridge.json
	eq(j.encode({ 1, 2, 3 }), "[1,2,3]")
	eq(j.encode({}), "{}", "empty table is an object")
	eq(j.encode(bridge.array({})), "[]", "bridge.array forces []")
	eq(j.encode(bridge.object({})), "{}")
	eq(j.encode({ b = 1, a = { true, "x" }, c = { d = bridge.null } }), '{"a":[true,"x"],"b":1,"c":{"d":null}}',
		"sorted keys, nesting")
	local ok, err = j.encode({ 1, a = 2 })
	eq(ok, nil, "mixed table")
	contains(err, "object keys must be strings")
	eq(j.encode({ [1] = "a", [3] = "c" }), nil, "sparse array")
	local cycle = {}
	cycle.self = cycle
	ok, err = j.encode(cycle)
	eq(ok, nil, "cycle")
	contains(err, "contains itself")
	eq(j.encode(print), nil, "function")
	-- the same table twice (not a cycle) is fine
	local shared = { 1 }
	eq(j.encode({ a = shared, b = shared }), '{"a":[1],"b":[1]}')
end

function tests.json_decodes(_, load)
	local bridge = load()
	local j = bridge.json
	local v = j.decode(' {"a": [1, 2.5, -3e2, true, false, null], "s": "x\\"y\\\\z\\/\\n", "o": {}} ')
	eq(v.a[1], 1)
	eq(v.a[2], 2.5)
	eq(v.a[3], -300)
	eq(v.a[4], true)
	eq(v.a[5], false)
	eq(v.a[6], bridge.null, "null")
	eq(v.s, 'x"y\\z/\n')
	eq(j.decode('"caf\\u00e9"'), "caf\195\169", "\\u escape")
	eq(j.decode('"\\ud83c\\udfae"'), "\240\159\142\174", "surrogate pair")
	eq(j.decode("0"), 0)
	eq(j.decode("-0.5"), -0.5)
end

function tests.json_round_trip_keeps_shapes(_, load)
	local j = load().json
	for _, text in ipairs({
		"[]",
		"{}",
		'{"a":[]}',
		'{"a":{}}',
		'[{"x":[1,2]},[],{}]',
		'{"k":null}',
		'{"s":"caf\195\169","u":"\240\159\142\174"}',
	}) do
		eq(j.encode(j.decode(text)), text, "round trip of " .. text)
	end
end

function tests.json_reports_invalid_input(_, load)
	local j = load().json
	local cases = {
		{ '{"a":}', "unexpected character '}' at position 6" },
		{ '{"a":1} x', "unexpected trailing data at position 9" },
		{ '"abc', "unterminated string at position 1" },
		{ '"\\ud83c"', "unpaired surrogate" },
		{ "01", "invalid number at position 1" },
		{ "-", "invalid number at position 1" },
		{ "1e", "invalid number at position 1" },
		{ "[1,]", "unexpected character ']'" },
		{ '{"a" 1}', "expected ':'" },
		{ "", "unexpected end of input" },
		{ '"a\1b"', "control character in string" },
		{ "1.", "invalid number at position 1" },
	}
	for _, case in ipairs(cases) do
		local value, err = j.decode(case[1])
		eq(value, nil, "decode " .. case[1])
		contains(err, case[2], "error for " .. case[1])
	end
	local value, err = j.decode(string.rep("[", 1000) .. string.rep("]", 1000))
	eq(value, nil, "deep nesting")
	contains(err, "nested too deeply")
	eq(j.decode(5), nil, "non-string input")
end

---------------------------------------------------------------------------
-- Availability and compatibility
---------------------------------------------------------------------------

function tests.version_constants(_, load)
	local bridge = load()
	eq(type(bridge.VERSION), "string")
	truthy(bridge.VERSION:match("^%d+%.%d+%.%d+$"), "VERSION is semver")
	eq(bridge.API_VERSION, 1)
end

function tests.plugin_absent_falls_back_cleanly(fake, load)
	fake.present = false
	local bridge = load()
	local ok, why = bridge.available()
	eq(ok, false)
	eq(why, "Lua Bridge plugin not available")
	eq(bridge.unavailable_reason(), "Lua Bridge plugin not available")
	eq(bridge.info(), nil)
	eq(bridge.has("websocket"), false)
	for _, call in ipairs({
		function() return bridge.register("x", { display_name = "X" }) end,
		function() return bridge.set_state("x", { a = 1 }) end,
		function() return bridge.emit("x", "e", {}) end,
		function() return bridge.run_command("x", "c", {}) end,
		function() return bridge.on_command("x", function() end) end,
		function() return bridge.on_event(function() end) end,
		function() return bridge.unregister("x") end,
	}) do
		local r, err = call()
		eq(r, false, "call result")
		eq(err, "Lua Bridge plugin not available")
	end
	eq(count(fake.connections), 0, "no signals connected")
	eq(fake.timer(), nil, "no heartbeat timer")
	eq(#fake.logs, 0, "nothing logged")
	eq(bridge.shutdown(), true)
end

function tests.compatible_plugin_is_available(fake, load)
	fake.api_version = 2 -- newer plugins just work
	local bridge = load()
	eq(bridge.available(), true)
	eq(bridge.unavailable_reason(), nil)
	eq(bridge.info().api_version, 2)
	eq(bridge.has("websocket"), true)
	eq(bridge.has("nope"), false)
	eq(#fake.calls_to("luabridge_get_info"), 1, "info requested once")
	bridge.available()
	eq(#fake.calls_to("luabridge_get_info"), 1, "info cached")
end

function tests.older_or_invalid_plugin_is_unavailable(fake, load)
	for _, case in ipairs({
		{ 0, "api_version 0 is older than this helper needs (1); update the Lua Bridge plugin" },
		{ nil, "no valid api_version" },
		{ "1", "no valid api_version" },
	}) do
		fake.api_version = case[1]
		fake.logs = {}
		local bridge = load()
		local ok, why = bridge.available()
		eq(ok, false, "available with api_version " .. tostring(case[1]))
		contains(why, case[2])
		local r, err = bridge.register("x", { display_name = "X" })
		eq(r, false)
		contains(err, case[2])
		bridge.available()
		eq(#fake.logs, 1, "warned once")
		eq(fake.logs[1].level, obslua.LOG_WARNING)
		contains(fake.logs[1].msg, case[2])
		eq(#fake.calls_to("luabridge_register"), 0, "nothing registered")
	end
end

---------------------------------------------------------------------------
-- Registration, state, commands, events
---------------------------------------------------------------------------

function tests.register_and_set_state_encode_json(fake, load)
	local bridge = load()
	eq(bridge.register("hello", { display_name = "Hello", commands = { { id = "ping" } } }), true)
	local reg = fake.calls_to("luabridge_register")[1]
	eq(reg.args.owner, "hello")
	eq(reg.args.json, '{"commands":[{"id":"ping"}],"display_name":"Hello"}')
	eq(bridge.set_state("hello", { n = 1, gone = bridge.null }), true)
	eq(fake.calls_to("luabridge_set_state")[1].args.json, '{"gone":null,"n":1}')
	eq(bridge.emit("hello", "e.x", { list = { { a = 1 } } }), true)
	eq(fake.calls_to("luabridge_emit")[1].args.json, '{"list":[{"a":1}]}')
	eq(bridge.run_command("hello", "ping"), true)
	eq(fake.calls_to("luabridge_run_command")[1].args.json, "{}", "no args")
	local r, err = bridge.register(5, {})
	eq(r, false)
	contains(err, "owner must be a string")
	r, err = bridge.set_state("hello", "x")
	eq(r, false)
	contains(err, "state must be a table")
	fake.fail_next.luabridge_set_state = "invalid state key 'bad-key'"
	r, err = bridge.set_state("hello", { ["bad-key"] = 1 })
	eq(r, false)
	eq(err, "invalid state key 'bad-key'", "plugin errors are returned unchanged")
end

function tests.commands_dispatch_by_owner(fake, load)
	local bridge = load()
	bridge.register("a", { display_name = "A" })
	bridge.register("b", { display_name = "B" })
	local got = {}
	eq(bridge.on_command("a", function(cmd, args, origin) got[#got + 1] = "a:" .. cmd .. ":" .. tostring(args.n) .. ":" .. origin end), true)
	eq(bridge.on_command("b", function(cmd) error("boom in b") end), true)
	eq(count(fake.connections), 1, "connected once")
	fake.send_command("a", "go", '{"n":5}', "dock")
	fake.send_command("b", "go", "{}", "websocket")
	fake.send_command("nobody", "go", "{}", "script")
	fake.send_command("a", "again", "not json", "script")
	eq(#got, 2)
	eq(got[1], "a:go:5:dock")
	eq(got[2], "a:again:nil:script", "bad JSON gives empty args")
	local logged = false
	for _, l in ipairs(fake.logs) do
		logged = logged or (l.msg:find("command handler for 'b' failed", 1, true) and l.msg:find("boom in b", 1, true))
	end
	truthy(logged, "handler error logged")
end

function tests.events_dispatch(fake, load)
	local bridge = load()
	local got = {}
	eq(bridge.on_event(function(owner, event, data) got[#got + 1] = owner .. ":" .. event .. ":" .. tostring(data.x) end),
		true)
	fake.send_event("other", "score.changed", '{"x":3}')
	eq(got[1], "other:score.changed:3")
end

---------------------------------------------------------------------------
-- Heartbeat and re-registration
---------------------------------------------------------------------------

function tests.heartbeat_timer(fake, load)
	local bridge = load()
	bridge.register("a", { display_name = "A" })
	local fn, ms = fake.timer()
	truthy(fn, "timer added")
	eq(ms, 10000, "default interval")
	bridge.register("b", { display_name = "B" }, { heartbeat_interval = 2 })
	fn, ms = fake.timer()
	eq(ms, 2000, "shortest interval wins")
	eq(count(fake.timers), 1, "one shared timer")
	bridge.register("c", { display_name = "C" }, { heartbeat = false })
	fn()
	local beats = {}
	for _, c in ipairs(fake.calls_to("luabridge_heartbeat")) do
		beats[c.args.owner] = true
	end
	truthy(beats.a and beats.b and not beats.c, "heartbeat for a and b only")

	eq(bridge.set_heartbeat("a", false), true)
	eq(bridge.set_heartbeat("b", false), true)
	eq(fake.timer(), nil, "timer removed when no owner beats")
	local before = #fake.calls_to("luabridge_heartbeat")
	eq(bridge.set_heartbeat("a", true), true)
	eq(#fake.calls_to("luabridge_heartbeat"), before + 1, "immediate heartbeat on resume")
	truthy(fake.timer(), "timer back")
	local r, err = bridge.set_heartbeat("nobody", true)
	eq(r, false)
	contains(err, "was not registered by this script")
end

function tests.heartbeat_callback(fake, load)
	local bridge = load()
	local seen = {}
	bridge.register("a", { display_name = "A" }, {
		on_heartbeat = function(ok, err) seen[#seen + 1] = tostring(ok) .. ":" .. tostring(err) end,
	})
	bridge.register("b", { display_name = "B" }, { on_heartbeat = function() error("boom") end })
	local tick = fake.timer()
	tick()
	eq(seen[1], "true:nil", "called after a successful heartbeat")
	fake.fail_next.luabridge_heartbeat = "some error"
	bridge.set_heartbeat("a", true)
	eq(seen[2], "false:some error", "called with the error")
	local logged = false
	for _, l in ipairs(fake.logs) do
		logged = logged or l.msg:find("heartbeat callback for 'b' failed", 1, true) ~= nil
	end
	truthy(logged, "callback error logged, not thrown")
end

function tests.reregisters_after_removal(fake, load)
	local now = 1000
	os.time = function() return now end
	local bridge = load()
	bridge.register("s", { display_name = "S" })
	bridge.set_state("s", { score = 2, label = "x" })
	bridge.set_state("s", { label = bridge.null })

	fake.owners.s = nil -- the dock's Remove button
	fake.logs = {}
	eq(bridge.set_state("s", { score = 3 }), true, "set_state succeeds after re-registration")
	eq(#fake.calls_to("luabridge_register"), 2, "registered again")
	local sets = fake.calls_to("luabridge_set_state")
	eq(sets[#sets - 1].args.json, '{"score":2}', "cached state republished (deleted key gone)")
	eq(sets[#sets].args.json, '{"score":3}', "then the call retried")
	contains(fake.logs[1].msg, "re-registered 's'")

	-- Guard: no second re-registration within 5 s
	fake.owners.s = nil
	now = now + 2
	local r, err = bridge.set_state("s", { score = 4 })
	eq(r, false)
	eq(err, "owner not registered")
	eq(#fake.calls_to("luabridge_register"), 2, "guarded")
	now = now + 5
	eq(bridge.set_state("s", { score = 4 }), true, "allowed again after 5 s")
	eq(#fake.calls_to("luabridge_register"), 3)

	-- Stale owner: the heartbeat registers it again
	now = now + 10
	fake.stale.s = true
	local tick = fake.timer()
	tick()
	eq(#fake.calls_to("luabridge_register"), 4, "stale owner re-registered by the heartbeat")

	-- Other errors are returned unchanged, without re-registering
	fake.fail_next.luabridge_emit = "invalid event name 'bad event'"
	r, err = bridge.emit("s", "bad event", {})
	eq(err, "invalid event name 'bad event'")
	eq(#fake.calls_to("luabridge_register"), 4)

	-- Owners this script never registered are not registered again
	r, err = bridge.set_state("someone.else", { a = 1 })
	eq(err, "owner not registered")
	eq(#fake.calls_to("luabridge_register"), 4)
end

function tests.shutdown_cleans_up(fake, load)
	local bridge = load()
	bridge.register("a", { display_name = "A" })
	bridge.register("b", { display_name = "B" })
	bridge.on_command("a", function() end)
	bridge.on_event(function() end)
	eq(count(fake.connections), 2)
	truthy(fake.timer(), "timer running")
	eq(bridge.shutdown(), true)
	eq(#fake.calls_to("luabridge_unregister"), 2, "both owners unregistered")
	eq(count(fake.connections), 0, "signals disconnected")
	eq(fake.timer(), nil, "timer removed")
	eq(bridge.shutdown(), true, "second shutdown")
	eq(#fake.calls_to("luabridge_unregister"), 2, "idempotent")
end

function tests.unregister_stops_heartbeat_and_dispatch(fake, load)
	local bridge = load()
	bridge.register("a", { display_name = "A" })
	local got = 0
	bridge.on_command("a", function() got = got + 1 end)
	eq(bridge.unregister("a"), true)
	eq(fake.timer(), nil, "no heartbeat for unregistered owner")
	fake.send_command("a", "go", "{}", "dock")
	eq(got, 0, "no dispatch after unregister")
end

return tests
