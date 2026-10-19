/*
Lua Bridge
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

// Per-script docks: the saved list of docks (docks.json in the plugin's config
// folder, the same for every profile and scene collection) and the decisions
// behind the "Lua Bridge Docks" window. Kept free of Qt and libobs so they are
// unit-tested.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "registry.hpp"

namespace luabridge::script_docks {

constexpr std::size_t max_docks = 32;
constexpr std::size_t max_title = limits::max_display_name; // bytes
constexpr int config_version = 1;

struct ScriptDock {
	std::string owner;
	std::string title;        // custom title; empty: the display name
	std::string display_name; // last display name seen, for the title before the script registers
};

struct Config {
	std::vector<ScriptDock> docks; // in the order they were added
};

struct ParseResult {
	Config config;
	bool ok = true;                    // false: the file is unusable (config is empty)
	std::string error;                 // why, when !ok
	std::vector<std::string> warnings; // entries skipped or fixed
};

// Never throws. Invalid entries are skipped with a warning; a file that isn't a
// version 1 config gives ok = false and an empty config.
ParseResult parse_config(std::string_view text);
std::string dump_config(const Config &config);

// The OBS dock id for an owner's dock, e.g. "luabridge.stopwatch". Owner ids are
// unique, so dock ids are too; the prefix keeps them apart from the combined
// dock ("lua-bridge") and from other plugins' docks.
std::string dock_id(std::string_view owner);
// The custom title, else the last display name seen, else the owner id
std::string dock_title(const ScriptDock &dock);
// A title as typed: control characters removed, spaces trimmed, at most
// max_title bytes (cut at a UTF-8 character boundary)
std::string clean_title(std::string_view title);

ScriptDock *find_dock(Config &config, std::string_view owner);
const ScriptDock *find_dock(const Config &config, std::string_view owner);

enum class AddResult { added, already_added, limit_reached, invalid_owner };
AddResult add_dock(Config &config, std::string_view owner, std::string_view display_name);
// False if the owner had no dock
bool remove_dock(Config &config, std::string_view owner);
// Records the display name a dock's script registered with. True if it changed.
bool note_display_name(Config &config, std::string_view owner, std::string_view display_name);

enum class OwnerStatus { running, not_responding, not_loaded };

struct ManagerRow {
	std::string owner;
	std::string display_name; // registered name, else the saved one, else the owner id
	OwnerStatus status = OwnerStatus::not_loaded;
	bool docked = false;
	std::string title; // custom title (empty: none)
};

// One row per registered owner and per docked owner that isn't registered,
// sorted by owner id
std::vector<ManagerRow> manager_rows(const Config &config, const std::vector<OwnerSummary> &owners);

} // namespace luabridge::script_docks
