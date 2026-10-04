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

#include "procs.hpp"
#include "signals.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

// TEMPORARY (remove in M3, when the dock sends commands): Tools menu item that
// sends "ping" to the "hello" owner registered by lua/examples/hello-bridge.lua
void on_send_test_ping_clicked(void *)
{
	luabridge::Result r = luabridge::registry().check_command("hello", "ping", "{}");
	if (!r.ok) {
		obs_log(LOG_WARNING, "test ping: %s", r.error.c_str());
		return;
	}
	obs_log(LOG_INFO, "test ping: emitting luabridge_command(hello, ping, {}, dock)");
	luabridge::signaling::emitter().command("hello", "ping", "{}", "dock");
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
	luabridge::register_procs(luabridge::signaling::emitter());
	luabridge::enable_procs();

	obs_frontend_add_tools_menu_item(obs_module_text("LuaBridge.Menu.SendTestPing"), on_send_test_ping_clicked,
					 nullptr);
	obs_frontend_add_event_callback(on_frontend_event, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	luabridge::signaling::emitter().ready(luabridge::info_json());
}

void obs_module_unload(void)
{
	// Procedures stay registered (OBS cannot remove them), so they must refuse
	// work from here on; other modules may still call them in their unload.
	luabridge::disable_procs();
	luabridge::signaling::stop();
	luabridge::registry().clear();
	// The frontend event callback is not removed: the frontend API is already
	// gone by now (it is torn down after OBS_FRONTEND_EVENT_EXIT).
	obs_log(LOG_INFO, "plugin unloaded");
}
