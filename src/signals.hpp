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

// Signals to scripts (spec B6). Emission is always queued to the UI thread with
// obs_queue_task, so scripts see signals in call order and never re-entrantly.

#include <string>

namespace luabridge::signals {

// Declares all luabridge_* signals on the global signal handler (obs_module_load)
void declare();

void emit_command(std::string owner, std::string command, std::string json, std::string origin);
void emit_event(std::string owner, std::string event, std::string json);
void emit_ready(std::string json);

// Once set, queued signals are dropped and nothing new is queued (OBS exit / unload)
void set_shutting_down(bool value);

} // namespace luabridge::signals
