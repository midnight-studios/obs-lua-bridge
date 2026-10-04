-- stopwatch-demo.lua: a small stopwatch controlled from the Lua Bridge dock
--
-- DEMO ONLY. This is not the StopWatch script from midnight-studios/obs-lua.
-- It implements the same Lua Bridge contract (owner "stopwatch"; see
-- docs/integrations/stopwatch.md), so dock layouts and websocket clients built
-- against it keep working with the real script once it adopts the
-- integration. Don't run this demo and the real StopWatch at the same time:
-- both use the owner "stopwatch".
--
-- With the plugin: a "Stopwatch (demo)" section in Docks > Lua Bridge with a
-- live display, Start/Pause (its label follows the state via label_bind),
-- Reset, and +/- seconds; websocket clients can send the same commands.
-- Without the plugin: use the buttons in this script's properties; the time
-- is shown in the chosen text source either way.

local bridge = dofile(script_path() .. "../luabridge.lua")
local obs = obslua

local OWNER = "stopwatch"
local TICK_MS = 200

local REGISTRATION = {
	display_name = "Stopwatch (demo)",
	commands = {
		{ id = "toggle", label = "Start", description = "Start or pause the stopwatch" },
		{ id = "start", label = "Start", description = "Start the stopwatch" },
		{ id = "pause", label = "Pause", description = "Pause the stopwatch" },
		{ id = "reset", label = "Reset", description = "Stop and set the time to 00:00:00", confirm = true },
		{ id = "add", label = "+ seconds", description = "Add seconds", args = { seconds = "int" } },
		{ id = "subtract", label = "- seconds", description = "Subtract seconds", args = { seconds = "int" } },
	},
	dock = {
		{ type = "label", bind = "display", style = "large" },
		{
			type = "row",
			items = {
				{ type = "button", command = "toggle", label_bind = "toggle_label" },
				{ type = "button", command = "reset" },
			},
		},
		{ type = "number", id = "step", label = "Seconds", min = 1, max = 3600, default = 10 },
		{
			type = "row",
			items = {
				{ type = "button", command = "subtract", args_from = { seconds = "step" } },
				{ type = "button", command = "add", args_from = { seconds = "step" } },
			},
		},
	},
}

local running = false
local accumulated_ns = 0 -- time counted before the current run
local started_ns = 0     -- os_gettime_ns() when the current run started
local shown_seconds = -1
local text_source = ""

local function elapsed_seconds()
	local ns = accumulated_ns
	if running then
		ns = ns + (obs.os_gettime_ns() - started_ns)
	end
	return math.floor(ns / 1e9)
end

local function format_time(seconds)
	return string.format("%02d:%02d:%02d", math.floor(seconds / 3600), math.floor(seconds / 60) % 60, seconds % 60)
end

local function set_text(source_name, text)
	if source_name == "" then
		return
	end
	local source = obs.obs_get_source_by_name(source_name)
	if source then
		local settings = obs.obs_data_create()
		obs.obs_data_set_string(settings, "text", text)
		obs.obs_source_update(source, settings)
		obs.obs_data_release(settings)
		obs.obs_source_release(source)
	end
end

-- Publishes the time (to the dock, websocket clients and the text source)
local function show(force)
	local seconds = elapsed_seconds()
	if seconds == shown_seconds and not force then
		return
	end
	shown_seconds = seconds
	local display = format_time(seconds)
	set_text(text_source, display)
	local toggle_label = bridge.null -- "Start"
	if running then
		toggle_label = "Pause"
	elseif seconds > 0 then
		toggle_label = "Resume"
	end
	bridge.set_state(OWNER, { display = display, elapsed = seconds, running = running, toggle_label = toggle_label })
end

local function start()
	if not running then
		running = true
		started_ns = obs.os_gettime_ns()
		show(true)
	end
end

local function pause()
	if running then
		accumulated_ns = accumulated_ns + (obs.os_gettime_ns() - started_ns)
		running = false
		show(true)
	end
end

local function reset()
	running = false
	accumulated_ns = 0
	show(true)
end

-- Adds (or with a negative value subtracts) seconds from the total, never below 0
local function adjust(seconds)
	local now = obs.os_gettime_ns()
	local total_ns = accumulated_ns + (running and (now - started_ns) or 0)
	accumulated_ns = math.max(0, total_ns + seconds * 1e9)
	started_ns = now -- a running stopwatch continues from the new total
	show(true)
end

local commands = {
	toggle = function()
		if running then
			pause()
		else
			start()
		end
	end,
	start = start,
	pause = pause,
	reset = reset,
	add = function(args)
		adjust(args.seconds or 0)
	end,
	subtract = function(args)
		adjust(-(args.seconds or 0))
	end,
}

local function tick()
	if running then
		show(false)
	end
end

function script_description()
	return "Lua Bridge for OBS: stopwatch demo (not the real StopWatch script). Controls are in "
		.. "Docks > Lua Bridge, or below when the plugin isn't installed."
end

function script_properties()
	local props = obs.obs_properties_create()
	local list = obs.obs_properties_add_list(props, "text_source", "Text source", obs.OBS_COMBO_TYPE_EDITABLE,
		obs.OBS_COMBO_FORMAT_STRING)
	local sources = obs.obs_enum_sources()
	if sources then
		for _, source in ipairs(sources) do
			local id = obs.obs_source_get_unversioned_id(source)
			if id == "text_gdiplus" or id == "text_ft2_source" then
				local name = obs.obs_source_get_name(source)
				obs.obs_property_list_add_string(list, name, name)
			end
		end
		obs.source_list_release(sources)
	end
	obs.obs_properties_add_button(props, "toggle", "Start / Pause", function()
		commands.toggle()
		return false
	end)
	obs.obs_properties_add_button(props, "reset", "Reset", function()
		reset()
		return false
	end)
	obs.obs_properties_add_button(props, "add10", "+10 s", function()
		adjust(10)
		return false
	end)
	obs.obs_properties_add_button(props, "sub10", "-10 s", function()
		adjust(-10)
		return false
	end)
	return props
end

function script_update(settings)
	text_source = obs.obs_data_get_string(settings, "text_source")
	show(true)
end

function script_load(settings)
	text_source = obs.obs_data_get_string(settings, "text_source")
	-- Both calls just return false when the plugin isn't installed
	bridge.register(OWNER, REGISTRATION)
	bridge.on_command(OWNER, function(command, args)
		local handler = commands[command]
		if handler then
			handler(args)
		end
	end)
	show(true)
	obs.timer_add(tick, TICK_MS)
end

function script_unload()
	obs.timer_remove(tick)
	bridge.shutdown()
end
