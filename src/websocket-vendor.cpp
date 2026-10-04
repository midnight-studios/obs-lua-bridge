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

#include "websocket-vendor.hpp"

#include <algorithm>
#include <atomic>
#include <shared_mutex>
#include <string>

#include <obs.h>
#include <plugin-support.h>

#include "event-data.hpp"
#include "procs.hpp"
#include "third-party/obs-websocket-api.h"

namespace luabridge::websocket {

namespace {

using json = nlohmann::json;

constexpr const char *vendor_name = "LuaBridge";

Emitter *emitter = nullptr;
obs_websocket_vendor vendor = nullptr;

// Events hold this shared while calling obs-websocket; stop() takes it
// exclusively, so once stop() returns no event is still being sent.
std::shared_mutex vendor_mutex;
std::atomic<bool> active{false};

json failure(const std::string &error)
{
	return {{"ok", false}, {"error", error}};
}

// Copies a JSON object into an obs_data response. obs_data drops null and
// arrays of non-objects, so responses are built without them.
void respond(obs_data_t *response, const json &result)
{
	if (!response)
		return;
	obs_data_t *data =
		obs_data_create_from_json(result.dump(-1, ' ', false, json::error_handler_t::replace).c_str());
	if (data) {
		obs_data_apply(response, data);
		obs_data_release(data);
	}
}

// Reads a required string field. Missing or non-string fields give an error.
bool string_field(obs_data_t *request, const char *name, std::string &out, std::string &error)
{
	obs_data_item_t *item = request ? obs_data_item_byname(request, name) : nullptr;
	if (!item) {
		error = std::string("missing ") + name;
		return false;
	}
	bool is_string = obs_data_item_gettype(item) == OBS_DATA_STRING;
	if (is_string) {
		const char *value = obs_data_item_get_string(item);
		out = value ? value : "";
	}
	obs_data_item_release(&item);
	if (!is_string) {
		error = std::string(name) + " must be a string";
		return false;
	}
	return true;
}

// Reads the optional "data" object as JSON text ("" when absent)
bool data_field(obs_data_t *request, std::string &out, std::string &error)
{
	obs_data_item_t *item = request ? obs_data_item_byname(request, "data") : nullptr;
	if (!item) {
		out.clear();
		return true;
	}
	bool is_object = obs_data_item_gettype(item) == OBS_DATA_OBJECT;
	if (is_object) {
		obs_data_t *obj = obs_data_item_get_obj(item);
		const char *text = obj ? obs_data_get_json(obj) : nullptr;
		out = text ? text : "{}";
		obs_data_release(obj);
	}
	obs_data_item_release(&item);
	if (!is_object) {
		error = "data must be an object";
		return false;
	}
	return true;
}

void log_failure(const char *request, const std::string &owner, const json &result)
{
	if (result.value("ok", false))
		return;
	obs_log(LOG_WARNING, "websocket %s(%.*s): %s", request, (int)std::min<size_t>(owner.size(), 64), owner.c_str(),
		result.value("error", std::string()).c_str());
}

json get_info(obs_data_t *)
{
	json info = json::parse(info_json());
	info["ok"] = true;
	return info;
}

json list_owners(obs_data_t *)
{
	json owners = json::array();
	for (const auto &o : registry().list_owners())
		owners.push_back({{"owner", o.id}, {"display_name", o.display_name}, {"stale", o.stale}});
	return {{"ok", true}, {"owners", std::move(owners)}};
}

json list_commands(obs_data_t *request)
{
	std::string owner, error;
	if (!string_field(request, "owner", owner, error))
		return failure(error);
	json commands;
	Result r = registry().get_commands(owner, commands);
	json result = r.ok ? json{{"ok", true}, {"owner", owner}, {"commands", std::move(commands)}} : failure(r.error);
	log_failure("ListCommands", owner, result);
	return result;
}

json get_state(obs_data_t *request)
{
	std::string owner, error;
	if (!string_field(request, "owner", owner, error))
		return failure(error);
	json state;
	Result r = registry().get_state(owner, state);
	json result = r.ok ? json{{"ok", true}, {"owner", owner}, {"state", std::move(state)}} : failure(r.error);
	log_failure("GetState", owner, result);
	return result;
}

json run_command(obs_data_t *request)
{
	std::string owner, command, data, error;
	json result;
	if (!string_field(request, "owner", owner, error) || !string_field(request, "command", command, error) ||
	    !data_field(request, data, error)) {
		result = failure(error);
	} else {
		std::string args;
		Result r = registry().check_command(owner, command, data, &args);
		if (r.ok) {
			emitter->command(owner, command, args, "websocket");
			result = {{"ok", true}, {"accepted", true}};
		} else {
			result = failure(r.error);
		}
	}
	log_failure("RunCommand", owner, result);
	return result;
}

// Request callbacks are called from C on obs-websocket's threads: never throw
template<json (*Handler)(obs_data_t *)> void handle(obs_data_t *request, obs_data_t *response, void *)
{
	json result;
	try {
		result = active ? Handler(request) : failure("plugin unloaded");
	} catch (const std::exception &e) {
		obs_log(LOG_ERROR, "websocket request failed: %s", e.what());
		result = failure("internal error");
	} catch (...) {
		obs_log(LOG_ERROR, "websocket request failed: unknown exception");
		result = failure("internal error");
	}
	try {
		respond(response, result);
	} catch (...) {
		obs_log(LOG_ERROR, "unexpected exception while answering a websocket request");
	}
}

void emit_event(const char *name, const json &data)
{
	std::shared_lock lock(vendor_mutex);
	if (!active || !vendor)
		return;
	obs_data_t *event_data =
		obs_data_create_from_json(data.dump(-1, ' ', false, json::error_handler_t::replace).c_str());
	if (!event_data)
		return;
	obs_websocket_vendor_emit_event(vendor, name, event_data);
	obs_data_release(event_data);
}

class WebsocketEvents final : public EventSink {
public:
	void state_changed(const std::string &owner, const std::string &changes_json) override
	{
		send("StateChanged", [&] { return event_data::state_changed(owner, changes_json); });
	}

	void custom_event(const std::string &owner, const std::string &event, const std::string &json_text) override
	{
		send("CustomEvent", [&] { return event_data::custom_event(owner, event, json_text); });
	}

private:
	template<typename Build> static void send(const char *name, Build build)
	{
		if (!active)
			return;
		try {
			emit_event(name, build());
		} catch (const std::exception &e) {
			obs_log(LOG_ERROR, "could not send %s: %s", name, e.what());
		} catch (...) {
			obs_log(LOG_ERROR, "could not send %s: unknown exception", name);
		}
	}
};

} // namespace

bool start(Emitter &signal_emitter)
{
	emitter = &signal_emitter;

	vendor = obs_websocket_register_vendor(vendor_name);
	if (!vendor) {
		obs_log(LOG_INFO, "obs-websocket not available; websocket requests and events disabled");
		return false;
	}

	struct {
		const char *name;
		obs_websocket_request_callback_function callback;
	} requests[] = {
		{"GetInfo", handle<get_info>},           {"ListOwners", handle<list_owners>},
		{"ListCommands", handle<list_commands>}, {"GetState", handle<get_state>},
		{"RunCommand", handle<run_command>},
	};
	for (const auto &req : requests) {
		if (!obs_websocket_vendor_register_request(vendor, req.name, req.callback, nullptr))
			obs_log(LOG_WARNING, "could not register websocket request %s", req.name);
	}

	active = true;
	obs_log(LOG_INFO, "obs-websocket vendor '%s' registered", vendor_name);
	return true;
}

void stop()
{
	std::unique_lock lock(vendor_mutex);
	active = false;
}

EventSink &events()
{
	static WebsocketEvents instance;
	return instance;
}

} // namespace luabridge::websocket
