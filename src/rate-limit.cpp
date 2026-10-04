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

#include "rate-limit.hpp"

#include <algorithm>
#include <cmath>

namespace luabridge {

namespace {
// Buckets that refilled completely are dropped once the map grows past this,
// so owner names from many past registrations can't accumulate
constexpr std::size_t prune_threshold = 512;
} // namespace

RateLimiter::RateLimiter(Config config, std::function<Clock::time_point()> now)
	: config_(config),
	  now_(std::move(now)),
	  global_{config.global_burst, now_()}
{
}

void RateLimiter::refill(Bucket &bucket, double rate, double burst, Clock::time_point now) const
{
	double elapsed = std::chrono::duration<double>(now - bucket.updated).count();
	if (elapsed > 0) {
		bucket.tokens = std::min(burst, bucket.tokens + elapsed * rate);
		bucket.updated = now;
	}
}

RateLimiter::Decision RateLimiter::acquire(std::string_view owner)
{
	std::lock_guard lock(mutex_);
	Clock::time_point now = now_();

	auto it = owners_.find(owner);
	if (it == owners_.end()) {
		if (owners_.size() >= prune_threshold) {
			for (auto p = owners_.begin(); p != owners_.end();) {
				refill(p->second, config_.owner_rate, config_.owner_burst, now);
				p = p->second.tokens >= config_.owner_burst ? owners_.erase(p) : std::next(p);
			}
		}
		it = owners_.emplace(std::string(owner), Bucket{config_.owner_burst, now}).first;
	}
	Bucket &bucket = it->second;
	refill(bucket, config_.owner_rate, config_.owner_burst, now);
	refill(global_, config_.global_rate, config_.global_burst, now);

	if (bucket.tokens >= 1.0 && global_.tokens >= 1.0) {
		bucket.tokens -= 1.0;
		global_.tokens -= 1.0;
		return {};
	}

	double wait_s = 0.0;
	if (bucket.tokens < 1.0)
		wait_s = std::max(wait_s, (1.0 - bucket.tokens) / config_.owner_rate);
	if (global_.tokens < 1.0)
		wait_s = std::max(wait_s, (1.0 - global_.tokens) / config_.global_rate);
	Decision d;
	d.allowed = false;
	d.retry_after_ms = std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(wait_s * 1000.0)));
	return d;
}

} // namespace luabridge
