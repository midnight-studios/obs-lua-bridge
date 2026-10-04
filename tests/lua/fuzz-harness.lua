-- fuzz-harness.lua: fuzzes every luabridge_* procedure from Lua (M5)
--
-- Load in Tools > Scripts (only when testing: it registers and removes many
-- owners and makes the plugin log deduplicated warnings). Calls every
-- procedure directly, bypassing the helper, with generated arguments: random
-- bytes (incl. invalid UTF-8), oversized JSON, wrong JSON types, unset
-- arguments and ids at the length limits. Deterministic (fixed seed).
-- Prints a RESULT line; the plugin must keep answering and every call must
-- return a boolean ok and a string error.

local obs = obslua

local SEED = 20261004
local ROUNDS = 10000 -- runs synchronously in script_load, so keep the UI freeze short
local PROCS = {
	{ "luabridge_get_info", {} },
	{ "luabridge_register", { "owner", "json" } },
	{ "luabridge_unregister", { "owner" } },
	{ "luabridge_set_state", { "owner", "json" } },
	{ "luabridge_emit", { "owner", "event", "json" } },
	{ "luabridge_heartbeat", { "owner" } },
	{ "luabridge_run_command", { "owner", "command", "json" } },
}

local function log(msg)
	obs.script_log(obs.LOG_INFO, msg)
end

-- Small deterministic PRNG (LuaJIT's math.random seeding differs by version)
local state = SEED
local function rand(n)
	state = (state * 1103515245 + 12345) % 2147483648
	return state % n
end

local function random_bytes(len)
	local t = {}
	for i = 1, len do
		t[i] = string.char(rand(256))
	end
	return table.concat(t)
end

local OWNERS = { "fuzz.a", "fuzz.b", "fuzz.c", "Bad Owner", "", string.rep("o", 64), string.rep("o", 65), "fuzz.\0x" }
local JSON = {
	'{"display_name":"Fuzz","commands":[{"id":"go","args":{"n":"int","s":"string"}}],"dock":[{"type":"label","bind":"k"}]}',
	'{"display_name":"F"}',
	"{}",
	"[]",
	"null",
	"5",
	'"str"',
	"{broken",
	'{"k":null}',
	'{"k":[1,2]}',
	'{"k":{"deep":true}}',
	'{"n":1.5,"s":"\\ud800"}',
	'{"n":1e400}',
	'{"display_name":"' .. string.rep("x", 200) .. '"}',
	"{" .. string.rep('"k":1,', 300) .. '"z":1}',
	'{"a":' .. string.rep("[", 3000) .. string.rep("]", 3000) .. "}",
	'{"t":"caf\195\169 \240\159\142\174 \226\128\174RTL"}',
	'{"bad":"\255\254"}',
}

local function random_arg()
	local roll = rand(10)
	if roll < 4 then
		return OWNERS[rand(#OWNERS) + 1]
	elseif roll < 7 then
		return JSON[rand(#JSON) + 1]
	elseif roll < 8 then
		return random_bytes(rand(64))
	elseif roll < 9 then
		return '{"s":"' .. string.rep("y", 65530 + rand(12)) .. '"}' -- around the 64 KiB limit
	end
	return nil -- leave the argument unset
end

local function call(name, args)
	local cd = obs.calldata_create()
	for _, key in ipairs(args) do
		local value = random_arg()
		if value ~= nil then
			obs.calldata_set_string(cd, key, value)
		end
	end
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), name, cd)
	local ok = obs.calldata_bool(cd, "ok")
	local err = obs.calldata_string(cd, "error")
	obs.calldata_destroy(cd)
	return found, ok, err
end

local function info_ok()
	local cd = obs.calldata_create()
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), "luabridge_get_info", cd)
	local ok = found and obs.calldata_bool(cd, "ok")
	obs.calldata_destroy(cd)
	return ok
end

local function cleanup()
	for _, owner in ipairs(OWNERS) do
		local cd = obs.calldata_create()
		obs.calldata_set_string(cd, "owner", owner)
		obs.proc_handler_call(obs.obs_get_proc_handler(), "luabridge_unregister", cd)
		obs.calldata_destroy(cd)
	end
end

function script_description()
	return "Lua Bridge for OBS: procedure fuzzer (testing only). Results go to the script log."
end

function script_load(settings)
	if not info_ok() then
		log("Lua Bridge plugin not available; nothing to fuzz")
		return
	end
	log(string.format("Lua Bridge procedure fuzzer: %d calls, seed %d", ROUNDS, SEED))
	local started = os.clock()
	local counts, problems = { ok = 0, failed = 0 }, 0
	for i = 1, ROUNDS do
		local proc = PROCS[rand(#PROCS) + 1]
		local found, ok, err = call(proc[1], proc[2])
		if not found then
			problems = problems + 1
			log("FAIL: " .. proc[1] .. " not found at call " .. i)
			break
		elseif ok then
			counts.ok = counts.ok + 1
		elseif type(err) == "string" and err ~= "" then
			counts.failed = counts.failed + 1
		else
			problems = problems + 1
			log("FAIL: " .. proc[1] .. " returned ok=false without an error at call " .. i)
		end
	end
	cleanup()
	local alive = info_ok()
	log(string.format("calls: %d ok, %d rejected with an error, %d problems; %.1f s; plugin still answering: %s",
		counts.ok, counts.failed, problems, os.clock() - started, tostring(alive)))
	log(string.format("RESULT: %d passed, %d failed", (problems == 0 and alive) and 1 or 0,
		(problems == 0 and alive) and 0 or 1))
end

function script_unload()
	cleanup()
end
