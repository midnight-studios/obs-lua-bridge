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

// obs-websocket vendor "LuaBridge" (spec B8): requests GetInfo, ListOwners,
// ListCommands, GetState, RunCommand and events StateChanged, CustomEvent.
// Request handlers run on obs-websocket's worker threads.

#include "emitter.hpp"
#include "event-sink.hpp"

namespace luabridge::websocket {

// Registers the vendor and its requests. Call from obs_module_post_load, after
// obs-websocket has loaded. Returns false (and logs once) if obs-websocket is
// not available; the plugin then works without it.
bool start(Emitter &emitter);

// Stops events and makes requests answer "plugin unloaded". Call on
// OBS_FRONTEND_EVENT_EXIT: obs-websocket unloads before this plugin, so the
// vendor must not be used after that.
void stop();

// Always valid; a no-op when obs-websocket is unavailable or stopped
EventSink &events();

} // namespace luabridge::websocket
