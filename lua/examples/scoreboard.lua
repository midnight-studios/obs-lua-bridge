-- scoreboard.lua: a minimal score board with Lua Bridge for OBS
--
-- With the plugin: a "Score Board" section in Docks > Lua Bridge (the score and
-- +/- buttons for each team, Reset). Every change is published as state (the
-- dock label updates) and emitted as the event "score.changed" {home, away},
-- which obs-websocket clients receive as a LuaBridge CustomEvent.
-- Without the plugin: use the buttons in this script's properties; the score
-- is shown in the chosen text source either way.
--
-- To run two score boards, copy this file under another name and give the
-- copy an Instance ID in its properties (e.g. "2" -> owner "scoreboard.2").

local bridge = dofile(script_path() .. "../luabridge.lua")
local obs = obslua

local BASE_OWNER = "scoreboard"
local owner = BASE_OWNER -- or "scoreboard.<instance id>" for a duplicated script
local registered = false
local EN_DASH = "\226\128\147"

local REGISTRATION = {
	display_name = "Score Board",
	commands = {
		{ id = "home_plus", label = "Home +" },
		{ id = "home_minus", label = "Home \226\136\146" },
		{ id = "away_plus", label = "Away +" },
		{ id = "away_minus", label = "Away \226\136\146" },
		{ id = "reset", label = "Reset", description = "Set both scores to 0", confirm = true },
	},
	dock = {
		{ type = "label", bind = "display", style = "large" },
		{
			type = "row",
			items = { { type = "button", command = "home_minus" }, { type = "button", command = "home_plus" } },
		},
		{
			type = "row",
			items = { { type = "button", command = "away_minus" }, { type = "button", command = "away_plus" } },
		},
		{ type = "button", command = "reset" },
	},
}


-- "scoreboard" for an empty instance ID, "scoreboard.2" for "2"; IDs may use a-z, 0-9,
-- _ and - (anything else is dropped)
local function instance_owner(id)
	local cleaned = (id or ""):lower():gsub("[^a-z0-9_-]", "")
	if cleaned == "" then
		return BASE_OWNER
	end
	return BASE_OWNER .. "." .. cleaned:sub(1, 64 - #BASE_OWNER - 1)
end

-- The registration for the current owner: duplicates get the instance ID in
-- their display name
local function registration_for(current)
	local registration = {}
	for k, v in pairs(REGISTRATION) do
		registration[k] = v
	end
	if current ~= BASE_OWNER then
		registration.display_name = REGISTRATION.display_name .. " " .. current:sub(#BASE_OWNER + 2)
	end
	return registration
end

local home, away = 0, 0
local text_source = ""

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

-- Publishes the score: state for the dock, an event for websocket clients and
-- other scripts, and the text source
local function publish(changed)
	local display = home .. " " .. EN_DASH .. " " .. away
	set_text(text_source, display)
	bridge.set_state(owner, { home = home, away = away, display = display })
	if changed then
		bridge.emit(owner, "score.changed", { home = home, away = away })
	end
end

local commands = {
	home_plus = function() home = home + 1 end,
	home_minus = function() home = math.max(0, home - 1) end,
	away_plus = function() away = away + 1 end,
	away_minus = function() away = math.max(0, away - 1) end,
	reset = function() home, away = 0, 0 end,
}

local function run(command)
	local before_home, before_away = home, away
	commands[command]()
	publish(home ~= before_home or away ~= before_away)
end

function script_description()
	return "Lua Bridge for OBS: score board example. Controls are in Docks > Lua Bridge, or below "
		.. "when the plugin isn't installed."
end

function script_properties()
	local props = obs.obs_properties_create()
	obs.obs_properties_add_text(props, "instance_id", "Instance ID (optional, for duplicated scripts)",
		obs.OBS_TEXT_DEFAULT)
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
	for _, id in ipairs({ "home_plus", "home_minus", "away_plus", "away_minus", "reset" }) do
		local label = id:gsub("_plus", " +"):gsub("_minus", " -"):gsub("^%l", string.upper)
		obs.obs_properties_add_button(props, id, label, function()
			run(id)
			return false
		end)
	end
	return props
end

local function on_command(command)
	if commands[command] then
		run(command)
	end
end

-- (Re-)registers under the owner for the configured instance ID. All calls just
-- return false when the plugin isn't installed.
local function apply_instance(settings)
	local wanted = instance_owner(obs.obs_data_get_string(settings, "instance_id"))
	if wanted == owner and registered then
		return
	end
	if registered then
		bridge.unregister(owner)
	end
	owner = wanted
	registered = bridge.register(owner, registration_for(owner))
	bridge.on_command(owner, on_command)
end

function script_update(settings)
	text_source = obs.obs_data_get_string(settings, "text_source")
	apply_instance(settings)
	publish(false)
end

function script_load(settings)
	text_source = obs.obs_data_get_string(settings, "text_source")
	apply_instance(settings)
	publish(false)
end

function script_unload()
	bridge.shutdown()
end
