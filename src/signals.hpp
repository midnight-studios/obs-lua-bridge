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

// Qt-based Emitter: every signal is posted with Qt::QueuedConnection to a
// QObject owned by the plugin, so it is delivered on the UI thread after the
// calling procedure returns, in FIFO order, whatever thread the call came from.
// (obs_queue_task(OBS_TASK_UI) is not used: it runs inline on the UI thread.)

#include "emitter.hpp"

namespace luabridge::signaling {

// Declares all luabridge_* signals on the global signal handler (obs_module_load)
void declare();

// Creates the plugin's QObject. Call on the UI thread in obs_module_load.
void start();
// Sets the shutting-down flag: nothing new is queued and queued emits are dropped
void begin_shutdown();
// Shuts down and deletes the QObject, which discards emits still in Qt's queue
void stop();

// Always valid; does nothing before start() and after begin_shutdown()
Emitter &emitter();

} // namespace luabridge::signaling
