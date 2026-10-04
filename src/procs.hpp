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

// Procedures on the global proc handler (spec B5 plus luabridge_run_command).

#include <string>

#include "registry.hpp"

namespace luabridge {

// The process-wide registry. It is never destroyed while OBS runs, because scripts
// may still call procedures after this module's obs_module_unload.
Registry &registry();

// JSON returned by luabridge_get_info and sent with luabridge_ready
std::string info_json();

void register_procs();

} // namespace luabridge
