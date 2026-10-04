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

// Owner/command/state registry and all input validation (spec B4, B7, B9).
// Pure C++17 with no libobs dependency, so it can be unit tested without OBS.
// All public methods are thread-safe and never throw.

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace luabridge {

using Clock = std::chrono::steady_clock;

namespace limits {
constexpr std::size_t max_json_bytes = 65536;
constexpr std::size_t max_id_length = 64;
constexpr std::size_t max_owners = 128;
constexpr std::size_t max_commands = 64;
constexpr std::size_t max_state_keys = 256;
constexpr std::size_t max_args = 16;
constexpr std::size_t max_dock_controls = 256;
constexpr std::size_t max_display_name = 128;
constexpr std::size_t max_label = 64;
constexpr std::size_t max_description = 256;
constexpr std::size_t max_text = 256;
constexpr auto stale_after = std::chrono::seconds(30);
} // namespace limits

// [a-z0-9_.-]{1,64}
bool is_valid_owner_id(std::string_view id);
// [a-z0-9_]{1,64}; also used for argument and dock control ids
bool is_valid_command_id(std::string_view id);
// [a-zA-Z0-9_.]{1,64}
bool is_valid_state_key(std::string_view key);
// Same rule as state keys
bool is_valid_event_name(std::string_view name);

enum class ArgType { Int, Number, String, Bool };

struct Command {
	std::string id;
	std::string label;
	std::string description;
	bool confirm = false;
	std::vector<std::pair<std::string, ArgType>> args;
};

struct Owner {
	std::string id;
	std::string display_name;
	std::vector<Command> commands;                   // declaration order
	nlohmann::json dock = nlohmann::json::array();   // validated controls, invalid ones removed
	std::map<std::string, nlohmann::json> state;     // scalar values only
	std::optional<Clock::time_point> last_heartbeat; // set by heartbeat() only
};

struct Result {
	bool ok = true;
	std::string error;
	std::vector<std::string> warnings;

	static Result failure(std::string message)
	{
		Result r;
		r.ok = false;
		r.error = std::move(message);
		return r;
	}
};

class Registry {
public:
	explicit Registry(std::function<Clock::time_point()> now = Clock::now);

	// Validates and stores a registration (B7). Replaces an existing one atomically;
	// on failure the previous registration is kept. Dock problems become warnings.
	Result register_owner(std::string_view owner, std::string_view json);
	// Idempotent: unregistering an unknown owner succeeds. *removed (if given) tells
	// whether a registration actually existed.
	Result unregister_owner(std::string_view owner, bool *removed = nullptr);
	// Merges scalar key/values (null deletes). On success *changes_json receives an
	// object with only the keys whose values changed.
	Result set_state(std::string_view owner, std::string_view json, std::string *changes_json);
	// Checks a luabridge_emit call. Empty json counts as {}.
	Result check_emit(std::string_view owner, std::string_view event, std::string_view json);
	// Checks that a command may be sent to a script. Empty json counts as {}.
	Result check_command(std::string_view owner, std::string_view command, std::string_view json);
	Result heartbeat(std::string_view owner);

	bool is_stale(std::string_view owner) const;
	std::size_t owner_count() const;
	// Removes all owners (plugin unload). The registry stays usable.
	void clear();

private:
	bool is_stale_locked(const Owner &owner) const;

	mutable std::mutex mutex_;
	std::map<std::string, Owner, std::less<>> owners_;
	std::function<Clock::time_point()> now_;
};

} // namespace luabridge
