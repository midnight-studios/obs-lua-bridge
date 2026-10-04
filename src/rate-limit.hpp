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

// Token-bucket rate limiting for websocket RunCommand (M5): one bucket per
// target owner plus one global bucket. obs-websocket does not tell a vendor
// which client sent a request, so limits are per owner, not per client.
// Pure C++17, thread-safe, unit-tested with an injectable clock.

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

#include "registry.hpp" // Clock

namespace luabridge {

class RateLimiter {
public:
	struct Config {
		double owner_rate = 30.0;  // commands per second, sustained
		double owner_burst = 60.0; // commands allowed at once
		double global_rate = 200.0;
		double global_burst = 400.0;
	};

	struct Decision {
		bool allowed = true;
		std::int64_t retry_after_ms = 0; // when not allowed: time until one more fits
	};

	explicit RateLimiter(Config config = Config{}, std::function<Clock::time_point()> now = Clock::now);

	// Takes one command for owner from the owner's and the global bucket, or
	// neither if either is empty
	Decision acquire(std::string_view owner);

	const Config &config() const { return config_; }

private:
	struct Bucket {
		double tokens;
		Clock::time_point updated;
	};
	void refill(Bucket &bucket, double rate, double burst, Clock::time_point now) const;

	Config config_;
	std::function<Clock::time_point()> now_;
	std::mutex mutex_;
	Bucket global_;
	std::map<std::string, Bucket, std::less<>> owners_;
};

} // namespace luabridge
