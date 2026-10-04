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

// Keeps repeated log lines from flooding the OBS log (M5). The first line for a
// key is logged; identical keys within the window are counted instead. The
// next occurrence after the window is logged again, together with how many
// were suppressed in between. Pure C++17, thread-safe, unit-tested.

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "registry.hpp" // Clock

namespace luabridge {

class LogLimiter {
public:
	struct Verdict {
		bool log = true;              // write this line now
		std::uint64_t suppressed = 0; // identical lines not written since the last one
	};

	explicit LogLimiter(Clock::duration window = std::chrono::seconds(10),
			    std::function<Clock::time_point()> now = Clock::now);

	Verdict check(std::string_view key);

	// " (N similar lines suppressed in the last 10 s)" or "" when nothing was suppressed
	static std::string suffix(const Verdict &verdict, Clock::duration window = std::chrono::seconds(10));

private:
	struct Entry {
		Clock::time_point logged;
		std::uint64_t suppressed;
	};

	Clock::duration window_;
	std::function<Clock::time_point()> now_;
	std::mutex mutex_;
	std::map<std::string, Entry, std::less<>> entries_;
};

} // namespace luabridge
