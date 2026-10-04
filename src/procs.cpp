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

#include "procs.hpp"

#include <algorithm>
#include <atomic>

#include <obs.h>
#include <plugin-support.h>

namespace luabridge {

namespace {

constexpr long long api_version = 1;

std::atomic<bool> procs_enabled{false};
Emitter *emitter = nullptr; // set once by register_procs, before any procedure can run

std::string_view arg(calldata_t *cd, const char *name)
{
	const char *s = calldata_string(cd, name);
	return s ? std::string_view(s) : std::string_view();
}

// Writes ok/error and logs failures and warnings with the calling procedure's name
void finish(calldata_t *cd, const char *proc, std::string_view owner, const Result &r)
{
	for (const auto &w : r.warnings)
		obs_log(LOG_WARNING, "%s(%.*s): %s", proc, (int)std::min<size_t>(owner.size(), 64), owner.data(),
			w.c_str());
	if (!r.ok)
		obs_log(LOG_WARNING, "%s(%.*s): %s", proc, (int)std::min<size_t>(owner.size(), 64), owner.data(),
			r.error.c_str());
	calldata_set_bool(cd, "ok", r.ok);
	calldata_set_string(cd, "error", r.error.c_str());
}

std::string json_or_empty_object(std::string_view json)
{
	return json.empty() ? std::string("{}") : std::string(json);
}

void proc_get_info(void *, calldata_t *cd)
{
	calldata_set_string(cd, "json", info_json().c_str());
	calldata_set_bool(cd, "ok", true);
	calldata_set_string(cd, "error", "");
}

void proc_register(void *, calldata_t *cd)
{
	auto owner = arg(cd, "owner");
	Result r = registry().register_owner(owner, arg(cd, "json"));
	if (r.ok)
		obs_log(LOG_INFO, "registered owner '%.*s'", (int)owner.size(), owner.data());
	finish(cd, "luabridge_register", owner, r);
}

void proc_unregister(void *, calldata_t *cd)
{
	auto owner = arg(cd, "owner");
	Result r = registry().unregister_owner(owner);
	if (r.ok)
		obs_log(LOG_INFO, "unregistered owner '%.*s'", (int)owner.size(), owner.data());
	finish(cd, "luabridge_unregister", owner, r);
}

void proc_set_state(void *, calldata_t *cd)
{
	auto owner = arg(cd, "owner");
	std::string changes;
	Result r = registry().set_state(owner, arg(cd, "json"), &changes);
	// changes feeds the websocket StateChanged event (M2) and the dock (M3)
	finish(cd, "luabridge_set_state", owner, r);
}

void proc_emit(void *, calldata_t *cd)
{
	auto owner = arg(cd, "owner");
	auto event = arg(cd, "event");
	auto json = arg(cd, "json");
	Result r = registry().check_emit(owner, event, json);
	if (r.ok)
		emitter->event(std::string(owner), std::string(event), json_or_empty_object(json));
	finish(cd, "luabridge_emit", owner, r);
}

void proc_heartbeat(void *, calldata_t *cd)
{
	auto owner = arg(cd, "owner");
	finish(cd, "luabridge_heartbeat", owner, registry().heartbeat(owner));
}

void proc_run_command(void *, calldata_t *cd)
{
	auto owner = arg(cd, "owner");
	auto command = arg(cd, "command");
	auto json = arg(cd, "json");
	Result r = registry().check_command(owner, command, json);
	if (r.ok)
		emitter->command(std::string(owner), std::string(command), json_or_empty_object(json), "script");
	finish(cd, "luabridge_run_command", owner, r);
}

// Procedures are called from C; never let an exception escape
template<void (*Proc)(void *, calldata_t *)> void safe(void *data, calldata_t *cd)
{
	if (!procs_enabled) {
		calldata_set_bool(cd, "ok", false);
		calldata_set_string(cd, "error", "plugin unloaded");
		return;
	}
	try {
		Proc(data, cd);
	} catch (...) {
		obs_log(LOG_ERROR, "unexpected exception in procedure");
		calldata_set_bool(cd, "ok", false);
		calldata_set_string(cd, "error", "internal error");
	}
}

} // namespace

Registry &registry()
{
	static Registry instance;
	return instance;
}

std::string info_json()
{
	nlohmann::json info = {
		{"api_version", api_version},
		{"plugin_version", PLUGIN_VERSION},
		{"obs_version", obs_get_version_string()},
		{"capabilities", {"commands", "state", "events", "heartbeat", "run_command"}},
	};
	return info.dump();
}

void enable_procs()
{
	procs_enabled = true;
}

void disable_procs()
{
	procs_enabled = false;
}

void register_procs(Emitter &signal_emitter)
{
	emitter = &signal_emitter;
	proc_handler_t *ph = obs_get_proc_handler();
	proc_handler_add(ph, "void luabridge_get_info(out bool ok, out string error, out string json)",
			 safe<proc_get_info>, nullptr);
	proc_handler_add(ph, "void luabridge_register(in string owner, in string json, out bool ok, out string error)",
			 safe<proc_register>, nullptr);
	proc_handler_add(ph, "void luabridge_unregister(in string owner, out bool ok, out string error)",
			 safe<proc_unregister>, nullptr);
	proc_handler_add(ph, "void luabridge_set_state(in string owner, in string json, out bool ok, out string error)",
			 safe<proc_set_state>, nullptr);
	proc_handler_add(
		ph,
		"void luabridge_emit(in string owner, in string event, in string json, out bool ok, out string error)",
		safe<proc_emit>, nullptr);
	proc_handler_add(ph, "void luabridge_heartbeat(in string owner, out bool ok, out string error)",
			 safe<proc_heartbeat>, nullptr);
	proc_handler_add(
		ph,
		"void luabridge_run_command(in string owner, in string command, in string json, out bool ok, out string error)",
		safe<proc_run_command>, nullptr);
}

} // namespace luabridge
