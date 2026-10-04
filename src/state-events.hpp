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

// Operations that combine the registry with event delivery, kept free of Qt and
// libobs so the "when is an event sent" rules are unit-tested with a fake sink.

#include <string_view>

#include "event-sink.hpp"
#include "registry.hpp"

namespace luabridge {

// luabridge_set_state: merges the state and, only if it succeeded and at least
// one key changed, calls events.state_changed with the registry's change set.
Result set_state_and_notify(Registry &registry, std::string_view owner, std::string_view json, EventSink &events);

} // namespace luabridge
