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

#include "event-data.hpp"

namespace luabridge::event_data {

using json = nlohmann::json;

json state_changed(std::string_view owner, std::string_view changes_json)
{
	// Keep the parsed object alive for the whole loop (items() only refers to it)
	const json parsed = json::parse(changes_json.begin(), changes_json.end());

	// obs_data drops null, so deleted keys go into "removed"
	json changes = json::object();
	json removed = json::object();
	for (const auto &[key, value] : parsed.items()) {
		if (value.is_null())
			removed[key] = true;
		else
			changes[key] = value;
	}

	json data = {{"owner", owner}, {"changes", std::move(changes)}};
	if (!removed.empty())
		data["removed"] = std::move(removed);
	return data;
}

json custom_event(std::string_view owner, std::string_view event, std::string_view json_text)
{
	return {{"owner", owner}, {"event", event}, {"data", json::parse(json_text.begin(), json_text.end())}};
}

} // namespace luabridge::event_data
