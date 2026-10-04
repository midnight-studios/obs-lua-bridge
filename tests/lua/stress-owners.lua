-- stress-owners.lua: ten owners for the stress and soak tests (M5)
--
-- Registers stress.0 ... stress.9, each with a "hit" command and a dock
-- section. Every hit increments that owner's "count" state (so the dock and
-- websocket clients see a StateChanged per command), and every 10th hit also
-- emits a "stress.tick" event. tests/websocket/stress.py and soak.py drive it
-- and compare "count" with the number of commands they sent.

local bridge = dofile(script_path() .. "../../lua/luabridge.lua")

local OWNERS = 10
local counts = {}

local function owner_id(i)
	return "stress." .. i
end

function script_description()
	return "Lua Bridge for OBS: ten owners for the stress and soak tests (testing only)."
end

function script_load(settings)
	if not bridge.available() then
		obslua.script_log(obslua.LOG_INFO, tostring(bridge.unavailable_reason()) .. "; nothing to do")
		return
	end
	for i = 0, OWNERS - 1 do
		local owner = owner_id(i)
		counts[owner] = 0
		bridge.register(owner, {
			display_name = "Stress " .. i,
			commands = { { id = "hit", args = { n = "int" } } },
			dock = {
				{ type = "label", bind = "count" },
				{ type = "label", bind = "last" },
			},
		})
		bridge.on_command(owner, function(command, args)
			if command ~= "hit" then
				return
			end
			counts[owner] = counts[owner] + 1
			bridge.set_state(owner, { count = counts[owner], last = "last n: " .. tostring(args.n) })
			if counts[owner] % 10 == 0 then
				bridge.emit(owner, "stress.tick", { count = counts[owner] })
			end
		end)
		bridge.set_state(owner, { count = 0, last = "last n: none" })
	end
	obslua.script_log(obslua.LOG_INFO, "registered " .. OWNERS .. " stress owners")
end

function script_unload()
	bridge.shutdown()
end
