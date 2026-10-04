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

#include "dock-events.hpp"

#include <functional>
#include <mutex>

#include <QMetaObject>

#include "lua-bridge-dock.hpp"

namespace luabridge::dock {

namespace {

// Guards the pointer so a post never races with detach_events()
std::mutex dock_mutex;
LuaBridgeDock *attached = nullptr;

void post(std::function<void(LuaBridgeDock &)> fn)
{
	std::lock_guard lock(dock_mutex);
	if (!attached)
		return;
	LuaBridgeDock *dock = attached;
	QMetaObject::invokeMethod(dock, [dock, fn = std::move(fn)] { fn(*dock); }, Qt::QueuedConnection);
}

class DockEvents final : public EventSink {
public:
	void state_changed(const std::string &owner, const std::string &changes_json) override
	{
		post([owner, changes_json](LuaBridgeDock &dock) { dock.apply_state_changes(owner, changes_json); });
	}
	void custom_event(const std::string &, const std::string &, const std::string &) override {}
	void owner_registered(const std::string &) override
	{
		post([](LuaBridgeDock &dock) { dock.schedule_reconcile(); });
	}
	void owner_unregistered(const std::string &) override
	{
		post([](LuaBridgeDock &dock) { dock.schedule_reconcile(); });
	}
};

} // namespace

void attach_events(LuaBridgeDock *dock)
{
	std::lock_guard lock(dock_mutex);
	attached = dock;
}

void detach_events()
{
	std::lock_guard lock(dock_mutex);
	attached = nullptr;
}

EventSink &events()
{
	static DockEvents instance;
	return instance;
}

} // namespace luabridge::dock
