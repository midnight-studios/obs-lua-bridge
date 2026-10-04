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

// Events for external clients (obs-websocket vendor events, spec B8), kept free
// of Qt and libobs. Implementations may be called from any thread and must not
// call back into the procedures.

#include <string>

namespace luabridge {

class EventSink {
public:
	virtual ~EventSink() = default;

	// changes_json is the registry's change set: changed keys with their new
	// values, deleted keys as null. Only called when it is non-empty.
	virtual void state_changed(const std::string &owner, const std::string &changes_json) = 0;
	// json is the event data object passed to luabridge_emit
	virtual void custom_event(const std::string &owner, const std::string &event, const std::string &json) = 0;
};

} // namespace luabridge
