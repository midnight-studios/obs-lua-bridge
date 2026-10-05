-- test_examples.lua: the example scripts never log a warning in normal use
-- (run by test_luabridge.py)
--
-- OBS opens its Script Log window whenever a script logs at warning level, so
-- the examples must stay quiet both without the plugin (users who haven't
-- installed it) and with it. Each test gets a fresh Lua state:
-- test(fake, load_helper, load_example).

local tests = {}

local function fail(message, level)
	error(message, (level or 1) + 1)
end

local function eq(actual, expected, what)
	if actual ~= expected then
		fail(string.format("%s: expected %s, got %s", what or "value", tostring(expected), tostring(actual)), 2)
	end
end

local function contains(text, part, what)
	if type(text) ~= "string" or not text:find(part, 1, true) then
		fail(string.format("%s: %q does not contain %q", what or "text", tostring(text), part), 2)
	end
end

local function no_warnings(fake, what)
	local warnings = fake.warnings()
	if #warnings > 0 then
		fail(string.format("%s: %d warning(s), first: %s", what, #warnings, warnings[1].msg), 2)
	end
end

-- Example file -> owner and the commands to send it when the plugin is present
local EXAMPLES = {
	["hello-bridge.lua"] = { owner = "hello", commands = { { "ping", "{}" } } },
	["stopwatch-demo.lua"] = {
		owner = "stopwatch",
		commands = {
			{ "toggle", "{}" }, { "add", '{"seconds":5}' }, { "subtract", '{"seconds":2}' },
			{ "pause", "{}" }, { "start", "{}" }, { "reset", "{}" }, { "nope", "{}" },
		},
	},
	["scoreboard.lua"] = {
		owner = "scoreboard",
		commands = {
			{ "home_plus", "{}" }, { "away_plus", "{}" }, { "home_minus", "{}" },
			{ "away_minus", "{}" }, { "reset", "{}" }, { "nope", "{}" },
		},
	},
	["ping-pong/ping.lua"] = { owner = "ping", commands = { { "send", "{}" }, { "send", "{}" }, { "nope", "{}" } } },
	["ping-pong/pong.lua"] = { owner = "pong", commands = { { "ping", '{"n":1}' }, { "ping", "{}" }, { "nope", "{}" } } },
}

local function fire_timers(fake)
	local due = {}
	for fn in pairs(fake.timers) do
		due[#due + 1] = fn
	end
	for _, fn in ipairs(due) do
		fn()
	end
end

-- What a user does with a script: load it, open its properties and click every
-- button, let time pass, change the Instance ID, unload it
local function exercise(fake, example)
	local settings = obslua.obs_data_create()
	script_description()
	script_load(settings)
	if script_properties then
		script_properties()
		for _, click in pairs(fake.buttons) do
			click()
		end
	end
	fake.now_ns = fake.now_ns + 3e9
	fire_timers(fake)
	for _, c in ipairs(example.commands) do
		fake.send_command(example.owner, c[1], c[2])
	end
	if script_update then
		obslua.obs_data_set_string(settings, "instance_id", "2")
		script_update(settings)
	end
	script_unload()
end

for file, example in pairs(EXAMPLES) do
	tests[file .. ": no warnings without the plugin"] = function(fake, _, load_example)
		fake.present = false
		load_example(file)
		exercise(fake, example)
		no_warnings(fake, file)
		eq(next(fake.connections), nil, "no signals connected")
		if file == "hello-bridge.lua" then
			eq(#fake.logs, 1, "hello-bridge logs once")
			eq(fake.logs[1].level, obslua.LOG_INFO)
			contains(fake.logs[1].msg, "not available; running without it")
		end
	end

	tests[file .. ": no warnings with the plugin"] = function(fake, _, load_example)
		load_example(file)
		exercise(fake, example)
		no_warnings(fake, file)
		eq(#fake.calls_to("luabridge_register") > 0, true, "registered")
	end
end

-- The one situation that is worth a warning: an incompatible plugin
function tests.incompatible_plugin_warns_once(fake, _, load_example)
	fake.api_version = 0
	load_example("stopwatch-demo.lua")
	exercise(fake, EXAMPLES["stopwatch-demo.lua"])
	eq(#fake.warnings(), 1, "one warning")
	contains(fake.warnings()[1].msg, "update the Lua Bridge plugin")
end

---------------------------------------------------------------------------
-- Ping-pong: commands one way, events back, no loops
---------------------------------------------------------------------------

-- The decoded JSON of every set_state / emit call for owner, in order
local function sent(fake, load_helper, proc, owner)
	local json = load_helper().json
	local found = {}
	for _, c in ipairs(fake.calls_to(proc)) do
		if c.args.owner == owner then
			found[#found + 1] = { event = c.args.event, data = json.decode(c.args.json) }
		end
	end
	return found
end

-- The value of key in owner's state after all set_state calls so far
local function state(fake, load_helper, owner, key)
	local value
	for _, s in ipairs(sent(fake, load_helper, "luabridge_set_state", owner)) do
		if s.data[key] ~= nil then
			value = s.data[key]
		end
	end
	return value
end

function tests.pong_answers_with_an_event_never_a_command(fake, load_helper, load_example)
	load_example("ping-pong/pong.lua")
	script_load(obslua.obs_data_create())
	fake.calls = {}
	fake.send_command("pong", "ping", '{"n":3}', "script")
	local events = sent(fake, load_helper, "luabridge_emit", "pong")
	eq(#events, 1, "one event")
	eq(events[1].event, "ponged")
	eq(events[1].data.n, 3, "n")
	eq(events[1].data.reply, "pong", "reply")
	eq(type(events[1].data.at), "number", "at")
	eq(state(fake, load_helper, "pong", "count"), 1, "count")
	eq(state(fake, load_helper, "pong", "pongs"), "Pongs: 1")
	eq(#fake.calls_to("luabridge_run_command"), 0, "pong never sends a command (no ping-pong loop)")
	no_warnings(fake, "pong")
	script_unload()
end

function tests.ping_without_pong_shows_pong_not_loaded(fake, load_helper, load_example)
	load_example("ping-pong/ping.lua")
	script_load(obslua.obs_data_create())
	for _ = 1, 3 do
		fake.send_command("ping", "send", "{}", "dock") -- pong isn't registered
	end
	eq(state(fake, load_helper, "ping", "result"), "pong not loaded")
	eq(state(fake, load_helper, "ping", "next_label"), "Send ping #1", "the number isn't used up")
	no_warnings(fake, "ping") -- no error, no Script Log warning

	fake.owners.pong = true -- pong is loaded now
	fake.send_command("ping", "send", "{}", "dock")
	eq(state(fake, load_helper, "ping", "result"), "Ping #1 sent")
	eq(state(fake, load_helper, "ping", "next_label"), "Send ping #2", "button caption via label_bind")
	local pings = fake.calls_to("luabridge_run_command")
	eq(pings[#pings].args.owner, "pong")
	eq(pings[#pings].args.command, "ping")
	contains(pings[#pings].args.json, '"n":1')
	script_unload()
end

function tests.ping_shows_ponged_events_and_never_answers_them(fake, load_helper, load_example)
	load_example("ping-pong/ping.lua")
	script_load(obslua.obs_data_create())
	local before = #fake.calls_to("luabridge_run_command")
	fake.send_event("pong", "ponged", '{"n":7,"reply":"pong","at":0}')
	contains(state(fake, load_helper, "ping", "last_pong"), 'Last pong: #7 "pong" at ')
	eq(#fake.calls_to("luabridge_run_command"), before, "no command sent from the event handler")

	-- Other events and other owners are ignored
	fake.send_event("pong", "something.else", '{"n":8}')
	fake.send_event("scoreboard", "ponged", '{"n":9}')
	contains(state(fake, load_helper, "ping", "last_pong"), "#7")
	no_warnings(fake, "ping")
	script_unload()
end

function tests.ping_pong_unload_disconnects_everything(fake, _, load_example)
	load_example("ping-pong/ping.lua")
	script_load(obslua.obs_data_create())
	eq(fake.connections.luabridge_event ~= nil, true, "listening to events")
	script_unload()
	eq(next(fake.connections), nil, "no signal handlers left after unload (safe to reload)")
	eq(fake.owners.ping, nil, "unregistered")
end

return tests
