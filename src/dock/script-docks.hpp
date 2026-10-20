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

// Per-script docks: one OBS dock per chosen owner, each a LuaBridgeDock limited
// to that owner. The choice is saved in docks.json in the plugin's config folder.
// UI thread only.
//
// OBS restores dock layouts once, after all modules have loaded, so saved docks
// are created in obs_module_load to get their position back. A dock added later
// (from the Lua Bridge Docks window) opens floating; OBS saves its layout from
// then on.

#include <map>
#include <set>
#include <string>

#include <QPointer>
#include <QString>

#include "emitter.hpp"
#include "event-sink.hpp"
#include "registry.hpp"
#include "script-docks-config.hpp"

namespace luabridge::dock {

class LuaBridgeDock;

class ScriptDocks {
public:
	ScriptDocks(Registry &registry, Emitter &emitter, EventSink &events);

	// Reads docks.json and creates the saved docks (obs_module_load)
	void load();
	const script_docks::Config &config() const { return config_; }

	// Adds and shows a dock for owner; returns "" or a message for the user
	QString add(const std::string &owner, const std::string &display_name);
	void remove(const std::string &owner);
	// title: as typed; empty restores the display name
	void set_title(const std::string &owner, const std::string &title);

	// OBS exit: removes the docks from OBS (their layout was already saved)
	void remove_all();

private:
	bool create(const script_docks::ScriptDock &dock);
	void destroy(const std::string &owner);
	void apply_title(const std::string &owner);
	void display_name_seen(const std::string &owner, const std::string &display_name);
	void save();

	Registry &registry_;
	Emitter &emitter_;
	EventSink &events_;
	std::string path_; // docks.json
	script_docks::Config config_;
	// OBS deletes a dock's widget when the dock is removed, so hold them weakly
	std::map<std::string, QPointer<LuaBridgeDock>> widgets_;
	std::set<std::string> seen_; // owners a script dock saw registered this session
};

} // namespace luabridge::dock
