/*
Lua Bridge for OBS
Copyright (C) 2026 Midnight Studios

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#pragma once

// Builds the data of the obs-websocket vendor events (spec B8) from the
// registry's JSON. Pure C++17 + nlohmann/json, so it is unit-tested without OBS.
// The results only contain shapes obs_data can carry (no null, no arrays of
// non-objects).

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace luabridge::event_data {

// StateChanged: {owner, changes:{key:value}, removed?:{key:true}}.
// changes_json is the registry's change set, where deleted keys are null.
// Throws nlohmann::json::exception if changes_json is not valid JSON.
nlohmann::json state_changed(std::string_view owner, std::string_view changes_json);

// CustomEvent: {owner, event, data}. json_text must be an object that
// Registry::check_emit accepted.
nlohmann::json custom_event(std::string_view owner, std::string_view event, std::string_view json_text);

} // namespace luabridge::event_data
