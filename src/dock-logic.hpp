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

// Decisions behind the dock (M3), kept free of Qt and libobs so they are
// unit-tested: which owner sections to create, rebuild or remove, how state
// values are shown, and how a button's arguments are built.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "registry.hpp"

namespace luabridge::dock_logic {

// A section currently shown in the dock
struct ShownSection {
	std::string owner;
	std::uint64_t generation = 0;
	bool stale = false;
};

struct SectionPlan {
	std::vector<std::string> remove;                         // shown, but no longer registered
	std::vector<OwnerSummary> create;                        // registered, not shown; append in this order
	std::vector<OwnerSummary> rebuild;                       // re-registered (new generation); rebuild in place
	std::vector<std::pair<std::string, bool>> stale_changes; // owner, now stale
};

// Reconciles the shown sections with the registry. New owners are created in
// registration order (by generation); existing sections keep their position.
SectionPlan plan_sections(const std::vector<ShownSection> &shown, const std::vector<OwnerSummary> &registered);

// Text for a label bound to a state value; nullptr or null means "not set"
std::string format_state_value(const nlohmann::json *value);

// The text shown for a state key that is not set (an em dash)
extern const char *const missing_value;

// True if a number control's min, max and default are all whole numbers
bool number_is_integer(const nlohmann::json &control);

// Builds the arguments a button sends. command is one entry from
// Registry::get_commands; input_values maps number/text control ids to their
// current value. Args whose control has no value are omitted. Whole numbers for
// "int" args become integers; anything else is passed on for check_command to
// validate.
nlohmann::json build_button_args(const nlohmann::json &button, const nlohmann::json &command,
				 const std::map<std::string, nlohmann::json> &input_values);

} // namespace luabridge::dock_logic
