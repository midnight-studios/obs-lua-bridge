-- fake_obslua.lua: a stand-in for OBS's obslua module and the Lua Bridge
-- plugin, for unit-testing lua/luabridge.lua outside OBS.
--
-- It defines the global obslua and returns a control table (fake) that tests
-- use to configure the plugin and inspect what the helper did.

local fake = {
	present = true,          -- false: the plugin's procedures don't exist
	api_version = 1,         -- reported by luabridge_get_info (may be a string or nil)
	owners = {},             -- owner id -> true while registered in the "plugin"
	stale = {},              -- owner id -> true: calls fail with "owner is stale"
	fail_next = {},          -- procedure name -> error to return once
	calls = {},              -- { name = ..., args = {...} } for every procedure call
	logs = {},               -- { level = ..., msg = ... }
	connections = {},        -- signal name -> connected function
	timers = {},             -- function -> interval ms
}

local function copy(t)
	local c = {}
	for k, v in pairs(t) do
		c[k] = v
	end
	return c
end

local function result(cd, ok, err)
	cd.bools.ok = ok
	cd.strings.error = err or ""
end

local owner_procs = {
	luabridge_set_state = true,
	luabridge_emit = true,
	luabridge_heartbeat = true,
	luabridge_run_command = true,
}

local function handle(name, cd)
	local args = cd.strings
	local owner = args.owner
	if fake.fail_next[name] then
		local err = fake.fail_next[name]
		fake.fail_next[name] = nil
		return result(cd, false, err)
	end
	if name == "luabridge_get_info" then
		local version = fake.api_version
		local version_json = version == nil and "" or ('"api_version":' .. (type(version) == "string" and
			('"' .. version .. '"') or tostring(version)) .. ",")
		cd.strings.json = "{" .. version_json .. '"capabilities":{"commands":true,"websocket":true}}'
		return result(cd, true)
	elseif name == "luabridge_register" then
		fake.owners[owner] = true
		fake.stale[owner] = nil
		return result(cd, true)
	elseif name == "luabridge_unregister" then
		fake.owners[owner] = nil
		return result(cd, true)
	elseif owner_procs[name] then
		if not fake.owners[owner] then
			return result(cd, false, "owner not registered")
		end
		if fake.stale[owner] and (name == "luabridge_heartbeat" or name == "luabridge_run_command") then
			return result(cd, false, "owner is stale; register again")
		end
		return result(cd, true)
	end
	return result(cd, false, "unknown procedure in fake")
end

obslua = {
	LOG_ERROR = 100,
	LOG_WARNING = 200,
	LOG_INFO = 300,
	calldata_create = function()
		return { strings = {}, bools = {} }
	end,
	calldata_destroy = function() end,
	calldata_set_string = function(cd, key, value)
		cd.strings[key] = value
	end,
	calldata_string = function(cd, key)
		return cd.strings[key]
	end,
	calldata_bool = function(cd, key)
		return cd.bools[key] or false
	end,
	obs_get_proc_handler = function()
		return "proc handler"
	end,
	obs_get_signal_handler = function()
		return "signal handler"
	end,
	proc_handler_call = function(_, name, cd)
		if not fake.present then
			return false
		end
		fake.calls[#fake.calls + 1] = { name = name, args = copy(cd.strings) }
		handle(name, cd)
		return true
	end,
	signal_handler_connect = function(_, name, fn)
		fake.connections[name] = fn
	end,
	signal_handler_disconnect = function(_, name, fn)
		if fake.connections[name] == fn then
			fake.connections[name] = nil
		end
	end,
	timer_add = function(fn, ms)
		fake.timers[fn] = ms
	end,
	timer_remove = function(fn)
		fake.timers[fn] = nil
	end,
	script_log = function(level, msg)
		fake.logs[#fake.logs + 1] = { level = level, msg = msg }
	end,
}

-- Calls made to a procedure (in order)
function fake.calls_to(name)
	local found = {}
	for _, c in ipairs(fake.calls) do
		if c.name == name then
			found[#found + 1] = c
		end
	end
	return found
end

-- The plugin delivers luabridge_command to the connected handler
function fake.send_command(owner, command, json, origin)
	local fn = fake.connections.luabridge_command
	if fn then
		fn({ strings = { owner = owner, command = command, json = json, origin = origin or "dock" }, bools = {} })
	end
end

function fake.send_event(owner, event, json)
	local fn = fake.connections.luabridge_event
	if fn then
		fn({ strings = { owner = owner, event = event, json = json }, bools = {} })
	end
end

-- The single active timer (function, interval) or nil
function fake.timer()
	for fn, ms in pairs(fake.timers) do
		return fn, ms
	end
end

return fake
