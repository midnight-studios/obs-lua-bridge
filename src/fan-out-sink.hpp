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

// An EventSink that forwards every notification to several sinks, in order
// (the obs-websocket vendor and the dock).

#include <initializer_list>
#include <vector>

#include "event-sink.hpp"

namespace luabridge {

class FanOutEventSink final : public EventSink {
public:
	FanOutEventSink(std::initializer_list<EventSink *> sinks) : sinks_(sinks) {}

	void state_changed(const std::string &owner, const std::string &changes_json) override
	{
		for (EventSink *sink : sinks_)
			sink->state_changed(owner, changes_json);
	}
	void custom_event(const std::string &owner, const std::string &event, const std::string &json) override
	{
		for (EventSink *sink : sinks_)
			sink->custom_event(owner, event, json);
	}
	void owner_registered(const std::string &owner) override
	{
		for (EventSink *sink : sinks_)
			sink->owner_registered(owner);
	}
	void owner_unregistered(const std::string &owner) override
	{
		for (EventSink *sink : sinks_)
			sink->owner_unregistered(owner);
	}

private:
	std::vector<EventSink *> sinks_;
};

} // namespace luabridge
