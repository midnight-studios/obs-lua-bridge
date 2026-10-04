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

// libFuzzer target for the core library (registry, event data, dock logic,
// rate and log limiting). Build with -DENABLE_FUZZING=ON (MSVC /fsanitize=fuzzer
// and AddressSanitizer); see docs/TESTING.md.
//
// Each input is a sequence of operations separated by 0xFF bytes. An operation
// is one selector byte followed by up to three arguments separated by 0x01
// (owner, name, json); arguments are passed as raw bytes, so invalid UTF-8,
// embedded NULs and oversized strings all reach the code under test.

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "dock-logic.hpp"
#include "event-data.hpp"
#include "log-limiter.hpp"
#include "rate-limit.hpp"
#include "registry.hpp"

using namespace luabridge;

namespace {

std::vector<std::string_view> split(std::string_view s, char sep, std::size_t max_parts)
{
	std::vector<std::string_view> parts;
	while (parts.size() + 1 < max_parts) {
		auto pos = s.find(sep);
		if (pos == std::string_view::npos)
			break;
		parts.push_back(s.substr(0, pos));
		s.remove_prefix(pos + 1);
	}
	parts.push_back(s);
	return parts;
}

// Invariants that must hold after every operation; a violation aborts, which
// libFuzzer reports as a crash with the input that caused it
void check(bool condition)
{
	if (!condition)
		std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
	auto now = std::make_shared<Clock::time_point>(Clock::time_point{} + std::chrono::hours(1));
	auto clock = [now] {
		return *now;
	};
	Registry reg(clock);
	RateLimiter rate(RateLimiter::Config{}, clock);
	LogLimiter log(std::chrono::seconds(10), clock);
	std::vector<dock_logic::ShownSection> shown;

	std::string_view input(reinterpret_cast<const char *>(data), size);
	for (std::string_view op : split(input, '\xff', 64)) {
		if (op.empty())
			continue;
		unsigned char selector = static_cast<unsigned char>(op[0]);
		auto args = split(op.substr(1), '\x01', 3);
		std::string_view owner = args[0];
		std::string_view name = args.size() > 1 ? args[1] : std::string_view();
		std::string_view json = args.size() > 2 ? args[2] : (args.size() > 1 ? args[1] : std::string_view());
		*now += std::chrono::milliseconds(selector * 37);

		switch (selector % 12) {
		case 0: {
			Result r = reg.register_owner(owner, json);
			check(r.ok || !r.error.empty());
			break;
		}
		case 1: {
			std::string changes;
			if (reg.set_state(owner, json, &changes).ok) {
				auto event =
					event_data::state_changed(owner, changes); // must not throw on registry output
				check(event.is_object());
			}
			break;
		}
		case 2: {
			std::string args_json;
			if (reg.check_command(owner, name, json, &args_json).ok)
				check(!args_json.empty() && args_json.front() == '{');
			break;
		}
		case 3:
			if (reg.check_emit(owner, name, json).ok)
				check(event_data::custom_event(owner, name, json.empty() ? "{}" : json).is_object());
			break;
		case 4:
			reg.heartbeat(owner);
			break;
		case 5: {
			bool removed = false;
			reg.unregister_owner(owner, &removed);
			break;
		}
		case 6: {
			nlohmann::json out;
			reg.get_commands(owner, out);
			reg.get_state(owner, out);
			reg.get_dock(owner, out);
			break;
		}
		case 7: {
			auto plan = dock_logic::plan_sections(shown, reg.list_owners());
			shown.clear();
			for (const auto &o : reg.list_owners())
				shown.push_back({o.id, o.generation, o.stale});
			check(plan.create.size() + plan.rebuild.size() <= limits::max_owners);
			break;
		}
		case 8:
			rate.acquire(owner);
			log.check(owner);
			break;
		case 9: {
			nlohmann::json value = nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
			if (!value.is_discarded()) {
				dock_logic::format_state_value(&value);
				dock_logic::button_text(std::string(name), &value);
				dock_logic::number_is_integer(value);
			}
			break;
		}
		case 10:
			*now += std::chrono::seconds(31); // heartbeats go stale
			break;
		default:
			reg.is_stale(owner);
			break;
		}
		check(reg.owner_count() <= limits::max_owners);
	}
	return 0;
}
