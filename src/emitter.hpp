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

// Delivery of signals to scripts (spec B6), kept free of Qt and libobs so code
// that only needs to send signals does not depend on how they are delivered.
// Implementations deliver asynchronously on the UI thread, after the calling
// procedure returns, in FIFO order, and drop everything once shutdown begins.

#include <string>

namespace luabridge {

class Emitter {
public:
	virtual ~Emitter() = default;

	// luabridge_command(owner, command, json, origin)
	virtual void command(std::string owner, std::string command, std::string json, std::string origin) = 0;
	// luabridge_event(owner, event, json)
	virtual void event(std::string owner, std::string event, std::string json) = 0;
	// luabridge_ready(json)
	virtual void ready(std::string json) = 0;
};

} // namespace luabridge
