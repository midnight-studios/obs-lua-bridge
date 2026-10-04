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

#include "state-events.hpp"

#include <string>

namespace luabridge {

Result set_state_and_notify(Registry &registry, std::string_view owner, std::string_view json, EventSink &events)
{
	std::string changes;
	Result r = registry.set_state(owner, json, &changes);
	if (r.ok && changes != "{}")
		events.state_changed(std::string(owner), changes);
	return r;
}

Result register_and_notify(Registry &registry, std::string_view owner, std::string_view json, EventSink &events)
{
	Result r = registry.register_owner(owner, json);
	if (r.ok)
		events.owner_registered(std::string(owner));
	return r;
}

Result unregister_and_notify(Registry &registry, std::string_view owner, EventSink &events, bool *removed)
{
	bool was_removed = false;
	Result r = registry.unregister_owner(owner, &was_removed);
	if (removed)
		*removed = was_removed;
	if (was_removed)
		events.owner_unregistered(std::string(owner));
	return r;
}

} // namespace luabridge
