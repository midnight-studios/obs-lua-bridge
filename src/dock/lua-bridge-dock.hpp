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

// The "Lua Bridge" dock (spec M3): one collapsible section per registered
// owner, a placeholder when there are none. A script dock is the same widget
// limited to one owner, with a placeholder while that owner isn't registered. Everything here runs on the UI
// thread; other threads reach it only through DockEvents (Qt::QueuedConnection).

#include <functional>
#include <map>
#include <string>

#include <QWidget>

#include "emitter.hpp"
#include "event-sink.hpp"
#include "registry.hpp"

class QLabel;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

namespace luabridge::dock {

class OwnerSection;

class LuaBridgeDock : public QWidget {
public:
	// events: where the dock's own Remove action is announced (all sinks,
	// including this dock's DockEvents and the websocket vendor). owner: show only
	// this owner (a script dock); empty: every owner (the combined dock).
	LuaBridgeDock(Registry &registry, Emitter &emitter, EventSink &events, std::string owner = {},
		      QWidget *parent = nullptr);

	// Script dock: called (UI thread) when its owner registers with a display
	// name different from the last one seen
	void on_display_name(std::function<void(const std::string &)> callback);
	// Script dock: its owner already registered earlier this session (a dock
	// re-added in the same session then says "not running", not "waiting")
	void set_owner_seen();

	// Rebuilds the sections from the registry on the next event-loop pass;
	// several calls before then collapse into one reconcile
	void schedule_reconcile();
	// changes_json: the registry's change set for owner (deleted keys are null)
	void apply_state_changes(const std::string &owner, const std::string &changes_json);
	// Closes open confirm dialogs without running their commands (OBS exit)
	void close_dialogs();

private:
	void reconcile();
	OwnerSection *build_section(const OwnerSummary &owner);
	std::string send_command(const std::string &owner, const std::string &command, const nlohmann::json &args);
	void remove_owner(const std::string &owner);
	void update_placeholder();

	Registry &registry_;
	Emitter &emitter_;
	EventSink &events_;

	const std::string owner_filter_; // empty: all owners
	bool owner_seen_ = false;        // script dock: its owner registered this session
	std::string display_name_;
	std::function<void(const std::string &)> display_name_callback_;

	QStackedWidget *stack_ = nullptr;
	QLabel *placeholder_ = nullptr;
	QScrollArea *scroll_ = nullptr;
	QVBoxLayout *sections_layout_ = nullptr; // sections, then a stretch

	std::map<std::string, OwnerSection *> sections_; // at most one section per owner
	std::map<std::string, bool> collapsed_;          // kept across reloads (per OBS session)
	bool reconcile_pending_ = false;
};

} // namespace luabridge::dock
