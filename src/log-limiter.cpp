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

#include "log-limiter.hpp"

namespace luabridge {

namespace {
constexpr std::size_t prune_threshold = 1024;
} // namespace

LogLimiter::LogLimiter(Clock::duration window, std::function<Clock::time_point()> now)
	: window_(window),
	  now_(std::move(now))
{
}

LogLimiter::Verdict LogLimiter::check(std::string_view key)
{
	std::lock_guard lock(mutex_);
	Clock::time_point now = now_();

	auto it = entries_.find(key);
	if (it == entries_.end()) {
		if (entries_.size() >= prune_threshold) {
			// Forget keys whose window ended with nothing suppressed
			for (auto e = entries_.begin(); e != entries_.end();)
				e = (now - e->second.logged >= window_ && e->second.suppressed == 0) ? entries_.erase(e)
												     : std::next(e);
		}
		entries_.emplace(std::string(key), Entry{now, 0});
		return {};
	}

	Entry &entry = it->second;
	if (now - entry.logged >= window_) {
		Verdict v{true, entry.suppressed};
		entry = Entry{now, 0};
		return v;
	}
	++entry.suppressed;
	return {false, 0};
}

std::string LogLimiter::suffix(const Verdict &verdict, Clock::duration window)
{
	if (verdict.suppressed == 0)
		return {};
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(window).count();
	return " (" + std::to_string(verdict.suppressed) + " similar line" + (verdict.suppressed == 1 ? "" : "s") +
	       " suppressed in the last " + std::to_string(seconds) + " s)";
}

} // namespace luabridge
