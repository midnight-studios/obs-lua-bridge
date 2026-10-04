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

#include "signals.hpp"

#include <atomic>
#include <functional>
#include <memory>

#include <obs.h>
#include <plugin-support.h>

namespace luabridge::signals {

namespace {

std::atomic<bool> shutting_down{false};

void run_task(void *param)
{
	std::unique_ptr<std::function<void()>> fn(static_cast<std::function<void()> *>(param));
	if (shutting_down)
		return;
	try {
		(*fn)();
	} catch (...) {
		obs_log(LOG_ERROR, "unexpected exception while emitting a signal");
	}
}

void queue(std::function<void()> fn)
{
	if (shutting_down)
		return;
	obs_queue_task(OBS_TASK_UI, run_task, new std::function<void()>(std::move(fn)), false);
}

void signal(const char *name, calldata_t *cd)
{
	signal_handler_signal(obs_get_signal_handler(), name, cd);
}

} // namespace

void declare()
{
	static const char *decls[] = {
		"void luabridge_command(string owner, string command, string json, string origin)",
		"void luabridge_event(string owner, string event, string json)",
		"void luabridge_ready(string json)",
		nullptr,
	};
	signal_handler_add_array(obs_get_signal_handler(), decls);
}

void emit_command(std::string owner, std::string command, std::string json, std::string origin)
{
	queue([owner = std::move(owner), command = std::move(command), json = std::move(json),
	       origin = std::move(origin)] {
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "owner", owner.c_str());
		calldata_set_string(&cd, "command", command.c_str());
		calldata_set_string(&cd, "json", json.c_str());
		calldata_set_string(&cd, "origin", origin.c_str());
		signal("luabridge_command", &cd);
		calldata_free(&cd);
	});
}

void emit_event(std::string owner, std::string event, std::string json)
{
	queue([owner = std::move(owner), event = std::move(event), json = std::move(json)] {
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "owner", owner.c_str());
		calldata_set_string(&cd, "event", event.c_str());
		calldata_set_string(&cd, "json", json.c_str());
		signal("luabridge_event", &cd);
		calldata_free(&cd);
	});
}

void emit_ready(std::string json)
{
	queue([json = std::move(json)] {
		calldata_t cd;
		calldata_init(&cd);
		calldata_set_string(&cd, "json", json.c_str());
		signal("luabridge_ready", &cd);
		calldata_free(&cd);
	});
}

void set_shutting_down(bool value)
{
	shutting_down = value;
}

} // namespace luabridge::signals
