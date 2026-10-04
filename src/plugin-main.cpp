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

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include "dock/dock-events.hpp"
#include "dock/lua-bridge-dock.hpp"
#include "fan-out-sink.hpp"
#include "procs.hpp"
#include "signals.hpp"
#include "websocket-vendor.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char *dock_id = "lua-bridge";
bool dock_added = false;
luabridge::dock::LuaBridgeDock *dock_widget = nullptr; // owned by OBS once added

// Notifications go to obs-websocket clients and to the dock
luabridge::EventSink &event_sinks()
{
	static luabridge::FanOutEventSink sinks{&luabridge::websocket::events(), &luabridge::dock::events()};
	return sinks;
}

void create_dock()
{
	auto *dock = new luabridge::dock::LuaBridgeDock(luabridge::registry(), luabridge::signaling::emitter(),
							event_sinks());
	if (!obs_frontend_add_dock_by_id(dock_id, obs_module_text("LuaBridge.Dock.Title"), dock)) {
		obs_log(LOG_WARNING, "could not add the Lua Bridge dock");
		delete dock;
		return;
	}
	dock_added = true;
	dock_widget = dock;
	luabridge::dock::attach_events(dock);
}

void remove_dock()
{
	luabridge::dock::detach_events();
	if (dock_added) {
		// An open confirm must not run its command (or outlive its parent) at exit
		dock_widget->close_dialogs();
		// OBS saved the dock layout before OBS_FRONTEND_EVENT_EXIT, so its
		// position is kept; removing it now destroys our widgets while the
		// plugin is still fully alive
		obs_frontend_remove_dock(dock_id);
		dock_added = false;
		dock_widget = nullptr;
	}
}

void on_frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		luabridge::signaling::emitter().ready(luabridge::info_json());
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		// No signals to scripts during shutdown (B9). Scripts are unloaded during
		// this event too; their luabridge_unregister calls still work.
		luabridge::signaling::begin_shutdown();
		// obs-websocket unloads before this plugin; stop using it now
		luabridge::websocket::stop();
		remove_dock();
		break;
	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	// Declared here so the signals exist before any script connects to them
	luabridge::signaling::declare();
	luabridge::signaling::start();
	luabridge::register_procs(luabridge::signaling::emitter(), event_sinks());
	luabridge::enable_procs();

	create_dock();
	obs_frontend_add_event_callback(on_frontend_event, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	// obs-websocket is loaded by now (vendors must register in post_load)
	luabridge::set_websocket_available(luabridge::websocket::start(luabridge::signaling::emitter()));
	luabridge::signaling::emitter().ready(luabridge::info_json());
}

void obs_module_unload(void)
{
	// Procedures stay registered (OBS cannot remove them), so they must refuse
	// work from here on; other modules may still call them in their unload.
	luabridge::disable_procs();
	luabridge::signaling::stop();
	luabridge::registry().clear();
	// The frontend event callback is not removed, and the dock was already
	// removed at OBS_FRONTEND_EVENT_EXIT: the frontend API is gone by now.
	obs_log(LOG_INFO, "plugin unloaded");
}
