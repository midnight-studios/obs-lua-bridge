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

return tests
