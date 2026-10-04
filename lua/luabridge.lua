-- luabridge.lua: helper library for Lua Bridge for OBS
-- https://github.com/midnight-studios/obs-lua-bridge  (GPL-2.0-or-later)
--
-- Copy this file next to your script and load it with
--
--     local bridge = dofile(script_path() .. "luabridge.lua")
--
-- It wraps the calldata boilerplate of the luabridge_* procedures and signals,
-- converts Lua tables to and from JSON, sends heartbeats, and registers your
-- owner again when the plugin has forgotten it (for example after Remove in the
-- dock). If the plugin is not installed, every call simply returns
-- false, "Lua Bridge plugin not available" and does nothing, so your script
-- keeps working without it.
--
-- Every function returns ok, err and never throws. Full reference: docs/API.md.
--
-- Lua 5.1 / LuaJIT compatible (the Lua that OBS uses).

local obs = obslua

local M = {}

M.VERSION = "1.0.0"
-- The plugin api_version this helper is written for. Newer plugins are fine
-- (API changes within a major version are additive); older ones are not.
M.API_VERSION = 1

local NOT_AVAILABLE = "Lua Bridge plugin not available"
local HEARTBEAT_INTERVAL_S = 10
local REREGISTER_GUARD_S = 5

---------------------------------------------------------------------------
-- JSON
---------------------------------------------------------------------------

-- JSON null (a table so it can be stored in tables; compare with == bridge.null)
local null = setmetatable({}, { __tostring = function() return "null" end })
M.null = null

-- Shape markers: arrays and objects decoded from JSON carry these, and
-- bridge.array{} / bridge.object{} set them explicitly (for empty tables).
local array_mt = { __luabridge_shape = "array" }
local object_mt = { __luabridge_shape = "object" }

function M.array(t)
	return setmetatable(t or {}, array_mt)
end

function M.object(t)
	return setmetatable(t or {}, object_mt)
end

local escapes = {
	['"'] = '\\"',
	["\\"] = "\\\\",
	["\b"] = "\\b",
	["\f"] = "\\f",
	["\n"] = "\\n",
	["\r"] = "\\r",
	["\t"] = "\\t",
}

local function encode_string(s)
	local escaped = s:gsub('[%c"\\]', function(c)
		return escapes[c] or string.format("\\u%04x", c:byte())
	end)
	return '"' .. escaped .. '"'
end

local function encode_number(n)
	if n ~= n or n == math.huge or n == -math.huge then
		error("cannot encode NaN or infinity", 0)
	end
	if n == math.floor(n) and math.abs(n) < 2 ^ 53 then
		return string.format("%.0f", n)
	end
	-- Shortest form that reads back as the same number
	for precision = 14, 17 do
		local s = string.format("%." .. precision .. "g", n)
		if tonumber(s) == n then
			return s
		end
	end
	return string.format("%.17g", n)
end

local function is_array(t)
	local mt = getmetatable(t)
	if mt == array_mt then
		return true
	elseif mt == object_mt then
		return false
	end
	local count = 0
	for k in pairs(t) do
		if type(k) ~= "number" or k < 1 or k ~= math.floor(k) then
			return false
		end
		count = count + 1
	end
	if count == 0 then
		return false -- an empty table is an object; use bridge.array{} for []
	end
	for i = 1, count do
		if t[i] == nil then
			return false
		end
	end
	return true
end

local encode_value

local function encode_table(t, seen)
	if seen[t] then
		error("cannot encode a table that contains itself", 0)
	end
	seen[t] = true
	local parts = {}
	if is_array(t) then
		for i = 1, #t do
			parts[i] = encode_value(t[i], seen)
		end
		seen[t] = nil
		return "[" .. table.concat(parts, ",") .. "]"
	end
	local keys = {}
	for k in pairs(t) do
		if type(k) ~= "string" then
			error("object keys must be strings (got " .. type(k) .. ")", 0)
		end
		keys[#keys + 1] = k
	end
	table.sort(keys)
	for i, k in ipairs(keys) do
		parts[i] = encode_string(k) .. ":" .. encode_value(t[k], seen)
	end
	seen[t] = nil
	return "{" .. table.concat(parts, ",") .. "}"
end

encode_value = function(v, seen)
	local kind = type(v)
	if v == nil or v == null then
		return "null"
	elseif kind == "boolean" then
		return v and "true" or "false"
	elseif kind == "number" then
		return encode_number(v)
	elseif kind == "string" then
		return encode_string(v)
	elseif kind == "table" then
		return encode_table(v, seen)
	end
	error("cannot encode a " .. kind, 0)
end

-- Decoder ------------------------------------------------------------------

local function decode_error(message, pos)
	error({ json_error = message .. " at position " .. pos }, 0)
end

local function skip_space(s, pos)
	return s:find("[^ \t\r\n]", pos) or #s + 1
end

local function utf8_char(cp)
	if cp < 0x80 then
		return string.char(cp)
	elseif cp < 0x800 then
		return string.char(0xC0 + math.floor(cp / 0x40), 0x80 + cp % 0x40)
	elseif cp < 0x10000 then
		return string.char(0xE0 + math.floor(cp / 0x1000), 0x80 + math.floor(cp / 0x40) % 0x40, 0x80 + cp % 0x40)
	end
	return string.char(
		0xF0 + math.floor(cp / 0x40000),
		0x80 + math.floor(cp / 0x1000) % 0x40,
		0x80 + math.floor(cp / 0x40) % 0x40,
		0x80 + cp % 0x40
	)
end

local simple_escapes = { ['"'] = '"', ["\\"] = "\\", ["/"] = "/", b = "\b", f = "\f", n = "\n", r = "\r", t = "\t" }

local function decode_string(s, pos)
	-- s:sub(pos, pos) == '"'
	local parts = {}
	local i = pos + 1
	while true do
		local j = s:find('["\\%c]', i)
		if not j then
			decode_error("unterminated string", pos)
		end
		parts[#parts + 1] = s:sub(i, j - 1)
		local c = s:sub(j, j)
		if c == '"' then
			return table.concat(parts), j + 1
		elseif c ~= "\\" then
			decode_error("control character in string", j)
		end
		local e = s:sub(j + 1, j + 1)
		if simple_escapes[e] then
			parts[#parts + 1] = simple_escapes[e]
			i = j + 2
		elseif e == "u" then
			local hex = s:match("^%x%x%x%x", j + 2)
			if not hex then
				decode_error("invalid \\u escape", j)
			end
			local cp = tonumber(hex, 16)
			i = j + 6
			if cp >= 0xD800 and cp <= 0xDBFF then
				local low = s:match("^\\u(%x%x%x%x)", i)
				local lo = low and tonumber(low, 16)
				if not lo or lo < 0xDC00 or lo > 0xDFFF then
					decode_error("unpaired surrogate", j)
				end
				cp = 0x10000 + (cp - 0xD800) * 0x400 + (lo - 0xDC00)
				i = i + 6
			elseif cp >= 0xDC00 and cp <= 0xDFFF then
				decode_error("unpaired surrogate", j)
			end
			parts[#parts + 1] = utf8_char(cp)
		else
			decode_error("invalid escape", j)
		end
	end
end

local decode_value

local function decode_array(s, pos, depth)
	local result = M.array({})
	pos = skip_space(s, pos + 1)
	if s:sub(pos, pos) == "]" then
		return result, pos + 1
	end
	while true do
		local value
		value, pos = decode_value(s, pos, depth)
		result[#result + 1] = value
		pos = skip_space(s, pos)
		local c = s:sub(pos, pos)
		if c == "]" then
			return result, pos + 1
		elseif c ~= "," then
			decode_error("expected ',' or ']'", pos)
		end
		pos = skip_space(s, pos + 1)
	end
end

local function decode_object(s, pos, depth)
	local result = M.object({})
	pos = skip_space(s, pos + 1)
	if s:sub(pos, pos) == "}" then
		return result, pos + 1
	end
	while true do
		if s:sub(pos, pos) ~= '"' then
			decode_error("expected a string key", pos)
		end
		local key
		key, pos = decode_string(s, pos)
		pos = skip_space(s, pos)
		if s:sub(pos, pos) ~= ":" then
			decode_error("expected ':'", pos)
		end
		local value
		value, pos = decode_value(s, skip_space(s, pos + 1), depth)
		result[key] = value
		pos = skip_space(s, pos)
		local c = s:sub(pos, pos)
		if c == "}" then
			return result, pos + 1
		elseif c ~= "," then
			decode_error("expected ',' or '}'", pos)
		end
		pos = skip_space(s, pos + 1)
	end
end

local MAX_DEPTH = 512

decode_value = function(s, pos, depth)
	depth = depth + 1
	if depth > MAX_DEPTH then
		decode_error("nested too deeply", pos)
	end
	local c = s:sub(pos, pos)
	if c == "{" then
		return decode_object(s, pos, depth)
	elseif c == "[" then
		return decode_array(s, pos, depth)
	elseif c == '"' then
		return decode_string(s, pos)
	elseif s:sub(pos, pos + 3) == "true" then
		return true, pos + 4
	elseif s:sub(pos, pos + 4) == "false" then
		return false, pos + 5
	elseif s:sub(pos, pos + 3) == "null" then
		return null, pos + 4
	end
	if c == "-" or c:find("%d") then
		local num = s:match("^-?%d*%.?%d*[eE]?[-+]?%d*", pos)
		local valid = num:find("^-?%d") and not num:find("^-?0%d") and not num:find("%.$") and not num:find("%.[eE]")
		local n = valid and tonumber(num)
		if not n then
			decode_error("invalid number", pos)
		end
		return n, pos + #num
	end
	if c == "" then
		decode_error("unexpected end of input", pos)
	end
	decode_error("unexpected character '" .. c .. "'", pos)
end

local json = {}

-- Returns the JSON text, or nil and an error message
function json.encode(value)
	local ok, result = pcall(encode_value, value, {})
	if ok then
		return result
	end
	return nil, tostring(result)
end

-- Returns the value (objects and arrays keep their shape when re-encoded;
-- JSON null is bridge.null), or nil and an error message
function json.decode(text)
	if type(text) ~= "string" then
		return nil, "json.decode expects a string"
	end
	local ok, result = pcall(function()
		local value, pos = decode_value(text, skip_space(text, 1), 0)
		pos = skip_space(text, pos)
		if pos <= #text then
			decode_error("unexpected trailing data", pos)
		end
		return value
	end)
	if ok then
		return result
	end
	if type(result) == "table" and result.json_error then
		return nil, result.json_error
	end
	return nil, tostring(result)
end

M.json = json

---------------------------------------------------------------------------
-- Plugin access
---------------------------------------------------------------------------

local function log(level, message)
	obs.script_log(level, "[luabridge] " .. message)
end

-- Calls a procedure with string arguments.
-- Returns found, ok, error, json (json is only set by luabridge_get_info).
local function call(proc, args)
	local cd = obs.calldata_create()
	for name, value in pairs(args) do
		obs.calldata_set_string(cd, name, value)
	end
	local found = obs.proc_handler_call(obs.obs_get_proc_handler(), proc, cd)
	local ok = found and obs.calldata_bool(cd, "ok") or false
	local err = found and (obs.calldata_string(cd, "error") or "") or NOT_AVAILABLE
	local text = found and obs.calldata_string(cd, "json") or nil
	obs.calldata_destroy(cd)
	return found, ok, err, text
end

local checked, available, reason, info = false, false, NOT_AVAILABLE, nil

-- Detects the plugin once and checks its api_version
local function check()
	if checked then
		return available, reason
	end
	checked = true
	local found, ok, err, text = call("luabridge_get_info", {})
	if not found then
		return false, reason -- not installed: fall back silently
	end
	if not ok then
		reason = "Lua Bridge plugin error: " .. tostring(err)
		log(obs.LOG_WARNING, reason)
		return false, reason
	end
	local decoded = json.decode(text or "")
	local version = type(decoded) == "table" and decoded.api_version or nil
	if type(version) ~= "number" then
		reason = "Lua Bridge plugin reports no valid api_version; update the Lua Bridge plugin"
	elseif version < M.API_VERSION then
		reason = string.format(
			"Lua Bridge plugin api_version %d is older than this helper needs (%d); update the Lua Bridge plugin",
			version,
			M.API_VERSION
		)
	else
		available, reason, info = true, nil, decoded
		return true, nil
	end
	log(obs.LOG_WARNING, reason)
	return false, reason
end

-- true if the plugin is installed and compatible (plus the reason if not)
function M.available()
	return check()
end

-- nil when available, otherwise why not
function M.unavailable_reason()
	local _, why = check()
	return why
end

-- The decoded luabridge_get_info table, or nil
function M.info()
	check()
	return info
end

-- true if the plugin reports this capability (e.g. "websocket")
function M.has(capability)
	check()
	return info ~= nil and type(info.capabilities) == "table" and info.capabilities[capability] == true
end

local function proc(name, args)
	local available_now, why = check()
	if not available_now then
		return false, why
	end
	local found, ok, err = call(name, args)
	if not found then
		return false, NOT_AVAILABLE
	end
	if not ok then
		return false, err
	end
	return true
end

---------------------------------------------------------------------------
-- Owners, heartbeat and re-registration
---------------------------------------------------------------------------

-- owner id -> { registration = json text, state = merged state table,
--               heartbeat = bool, interval = seconds, last_reregister = time }
local owners = {}
local command_handlers = {} -- owner id -> function(command, args, origin)
local event_handlers = {}   -- list of function(owner, event, data)
local command_connected, event_connected = false, false
local timer_interval_ms = nil
local heartbeat_tick

local function retryable(err)
	return err == "owner not registered" or err == "owner is stale; register again"
end

local function encode_arg(value, what)
	if value == nil then
		return "{}"
	end
	if type(value) ~= "table" then
		return nil, what .. " must be a table"
	end
	local text, err = json.encode(value)
	if not text then
		return nil, "invalid " .. what .. ": " .. err
	end
	return text
end

-- Registers a known owner again and republishes its last state
local function reregister(owner)
	local o = owners[owner]
	if not o then
		return false
	end
	local now = os.time()
	if o.last_reregister and now - o.last_reregister < REREGISTER_GUARD_S then
		return false
	end
	o.last_reregister = now
	if not proc("luabridge_register", { owner = owner, json = o.registration }) then
		return false
	end
	log(obs.LOG_INFO, "re-registered '" .. owner .. "'")
	if next(o.state) ~= nil then
		local text = json.encode(o.state)
		if text then
			proc("luabridge_set_state", { owner = owner, json = text })
		end
	end
	return true
end

-- Runs fn (returning ok, err); if the owner was forgotten, registers it again
-- and retries once
local function with_reregistration(owner, fn)
	local ok, err = fn()
	if ok or not retryable(err) then
		return ok, err
	end
	if not reregister(owner) then
		return ok, err
	end
	return fn()
end

local function update_timer()
	local interval = nil
	for _, o in pairs(owners) do
		if o.heartbeat and (not interval or o.interval < interval) then
			interval = o.interval
		end
	end
	local ms = interval and math.floor(interval * 1000) or nil
	if ms == timer_interval_ms then
		return
	end
	if timer_interval_ms then
		obs.timer_remove(heartbeat_tick)
	end
	timer_interval_ms = ms
	if ms then
		obs.timer_add(heartbeat_tick, ms)
	end
end

local function send_heartbeat(owner)
	local ok, err = with_reregistration(owner, function()
		return proc("luabridge_heartbeat", { owner = owner })
	end)
	local o = owners[owner]
	if o and o.on_heartbeat then
		local called, cb_err = pcall(o.on_heartbeat, ok, err)
		if not called then
			log(obs.LOG_WARNING, "heartbeat callback for '" .. owner .. "' failed: " .. tostring(cb_err))
		end
	end
	return ok, err
end

heartbeat_tick = function()
	for owner, o in pairs(owners) do
		if o.heartbeat then
			send_heartbeat(owner)
		end
	end
end

local function on_command_signal(cd)
	local owner = obs.calldata_string(cd, "owner")
	local handler = command_handlers[owner]
	if not handler then
		return
	end
	local command = obs.calldata_string(cd, "command")
	local origin = obs.calldata_string(cd, "origin")
	local args = json.decode(obs.calldata_string(cd, "json") or "{}")
	if type(args) ~= "table" then
		args = M.object({})
	end
	local ok, err = pcall(handler, command, args, origin)
	if not ok then
		log(obs.LOG_WARNING, "command handler for '" .. tostring(owner) .. "' failed: " .. tostring(err))
	end
end

local function on_event_signal(cd)
	local owner = obs.calldata_string(cd, "owner")
	local event = obs.calldata_string(cd, "event")
	local data = json.decode(obs.calldata_string(cd, "json") or "{}")
	if type(data) ~= "table" then
		data = M.object({})
	end
	for _, handler in ipairs(event_handlers) do
		local ok, err = pcall(handler, owner, event, data)
		if not ok then
			log(obs.LOG_WARNING, "event handler failed: " .. tostring(err))
		end
	end
end

---------------------------------------------------------------------------
-- Public API
---------------------------------------------------------------------------

-- Declares an owner: registration is the table from docs/API.md
-- ({display_name=..., commands={...}, dock={...}}). options:
--   heartbeat = true          send heartbeats (default true)
--   heartbeat_interval = 10   seconds
--   on_heartbeat = fn(ok, err) called after each heartbeat (e.g. to show it)
-- Registering clears the owner's state in the plugin (and in this helper).
function M.register(owner, registration, options)
	if type(owner) ~= "string" then
		return false, "owner must be a string"
	end
	local text, err = encode_arg(registration, "registration")
	if not text then
		return false, err
	end
	local ok
	ok, err = proc("luabridge_register", { owner = owner, json = text })
	if not ok then
		return false, err
	end
	options = options or {}
	owners[owner] = {
		registration = text,
		state = {},
		heartbeat = options.heartbeat ~= false,
		interval = tonumber(options.heartbeat_interval) or HEARTBEAT_INTERVAL_S,
		on_heartbeat = type(options.on_heartbeat) == "function" and options.on_heartbeat or nil,
	}
	update_timer()
	return true
end

-- Removes an owner (from script_unload, or when the script no longer needs it)
function M.unregister(owner)
	local was_known = owners[owner] ~= nil
	owners[owner] = nil
	command_handlers[owner] = nil
	if was_known then
		update_timer()
	end
	return proc("luabridge_unregister", { owner = owner })
end

-- Calls fn(command, args, origin) for each command sent to owner. args is a
-- table; origin is "dock", "websocket" or "script".
function M.on_command(owner, fn)
	local available_now, why = check()
	if not available_now then
		return false, why
	end
	if type(fn) ~= "function" then
		return false, "handler must be a function"
	end
	command_handlers[owner] = fn
	if not command_connected then
		obs.signal_handler_connect(obs.obs_get_signal_handler(), "luabridge_command", on_command_signal)
		command_connected = true
	end
	return true
end

-- Calls fn(owner, event, data) for every luabridge_emit from any owner
function M.on_event(fn)
	local available_now, why = check()
	if not available_now then
		return false, why
	end
	if type(fn) ~= "function" then
		return false, "handler must be a function"
	end
	event_handlers[#event_handlers + 1] = fn
	if not event_connected then
		obs.signal_handler_connect(obs.obs_get_signal_handler(), "luabridge_event", on_event_signal)
		event_connected = true
	end
	return true
end

-- Merges values into the owner's state; bridge.null deletes a key
function M.set_state(owner, values)
	local text, err = encode_arg(values, "state")
	if not text then
		return false, err
	end
	local ok
	ok, err = with_reregistration(owner, function()
		return proc("luabridge_set_state", { owner = owner, json = text })
	end)
	local o = owners[owner]
	if ok and o then
		for k, v in pairs(values) do
			if v == null then
				o.state[k] = nil
			else
				o.state[k] = v
			end
		end
	end
	return ok, err
end

-- Sends a custom event to other scripts and websocket clients
function M.emit(owner, event, data)
	local text, err = encode_arg(data, "event data")
	if not text then
		return false, err
	end
	return with_reregistration(owner, function()
		return proc("luabridge_emit", { owner = owner, event = event, json = text })
	end)
end

-- Sends a command to any owner (origin "script")
function M.run_command(owner, command, args)
	local text, err = encode_arg(args, "args")
	if not text then
		return false, err
	end
	return with_reregistration(owner, function()
		return proc("luabridge_run_command", { owner = owner, command = command, json = text })
	end)
end

-- Turns this script's heartbeat for owner on or off. Turning it on sends one
-- at once (which registers the owner again if the plugin had forgotten it).
function M.set_heartbeat(owner, enabled)
	local o = owners[owner]
	if not o then
		return false, "owner '" .. tostring(owner) .. "' was not registered by this script"
	end
	o.heartbeat = enabled and true or false
	update_timer()
	if o.heartbeat then
		return send_heartbeat(owner)
	end
	return true
end

-- Unregisters every owner of this script, stops the heartbeat and disconnects
-- the signals. Call it from script_unload. Safe to call more than once.
function M.shutdown()
	for owner in pairs(owners) do
		proc("luabridge_unregister", { owner = owner })
	end
	owners = {}
	command_handlers = {}
	event_handlers = {}
	update_timer()
	local sh = obs.obs_get_signal_handler()
	if command_connected then
		obs.signal_handler_disconnect(sh, "luabridge_command", on_command_signal)
		command_connected = false
	end
	if event_connected then
		obs.signal_handler_disconnect(sh, "luabridge_event", on_event_signal)
		event_connected = false
	end
	return true
end

return M
