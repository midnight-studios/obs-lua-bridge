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

// The dock's EventSink. Notifications arrive on any thread (scripts, Lua timers,
// obs-websocket); each is copied and posted to the dock with
// Qt::QueuedConnection, so widgets are only ever touched on the UI thread.

#include "event-sink.hpp"

namespace luabridge::dock {

class LuaBridgeDock;

// Starts forwarding to dock (UI thread, after the dock is created)
void attach_events(LuaBridgeDock *dock);
// Stops forwarding; call on the UI thread before the dock is destroyed.
// Calls already posted to the dock are dropped by Qt when it is deleted.
void detach_events();

// Always valid; does nothing while no dock is attached
EventSink &events();

} // namespace luabridge::dock
