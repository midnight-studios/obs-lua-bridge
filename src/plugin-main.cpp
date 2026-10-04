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

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr long long API_VERSION = 1;

// void luabridge_get_info(out bool ok, out string error, out string json)
void proc_get_info(void *, calldata_t *cd)
{
	obs_data_t *info = obs_data_create();
	obs_data_set_int(info, "api_version", API_VERSION);
	obs_data_set_string(info, "plugin_version", PLUGIN_VERSION);
	obs_data_set_string(info, "obs_version", obs_get_version_string());
	obs_data_array_t *capabilities = obs_data_array_create();
	obs_data_set_array(info, "capabilities", capabilities);
	obs_data_array_release(capabilities);

	// calldata_set_string copies, so the JSON can be released afterwards
	calldata_set_string(cd, "json", obs_data_get_json(info));
	calldata_set_bool(cd, "ok", true);
	calldata_set_string(cd, "error", "");
	obs_data_release(info);
}

void emit_command(const char *owner, const char *command, const char *json, const char *origin)
{
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "owner", owner);
	calldata_set_string(&cd, "command", command);
	calldata_set_string(&cd, "json", json);
	calldata_set_string(&cd, "origin", origin);
	signal_handler_signal(obs_get_signal_handler(), "luabridge_command", &cd);
	calldata_free(&cd);
}

// M0 spike: temporary Tools menu item that sends a test command to the "hello" owner
void send_test_ping(void *)
{
	obs_log(LOG_INFO, "test ping: emitting luabridge_command(hello, ping, {}, dock)");
	emit_command("hello", "ping", "{}", "dock");
}

void on_send_test_ping_clicked(void *)
{
	// Signals to scripts are always emitted on the UI thread
	obs_queue_task(OBS_TASK_UI, send_test_ping, nullptr, false);
}

} // namespace

bool obs_module_load(void)
{
	// Declared here so the signal exists before any script connects to it
	signal_handler_add(obs_get_signal_handler(),
			   "void luabridge_command(string owner, string command, string json, string origin)");

	proc_handler_add(obs_get_proc_handler(),
			 "void luabridge_get_info(out bool ok, out string error, out string json)", proc_get_info,
			 nullptr);

	obs_frontend_add_tools_menu_item(obs_module_text("LuaBridge.Menu.SendTestPing"), on_send_test_ping_clicked,
					 nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
