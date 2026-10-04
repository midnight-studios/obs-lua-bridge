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

// One collapsible section of the Lua Bridge dock: the controls of one owner
// (spec B7). UI thread only.

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <QFrame>

#include <nlohmann/json.hpp>

#include "registry.hpp"

class QCheckBox;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QWidget;

namespace luabridge::dock {

class OwnerSection : public QFrame {
public:
	struct Callbacks {
		// Sends a command for this owner; returns the error, or "" on success
		std::function<std::string(const std::string &command, const nlohmann::json &args)> send;
		std::function<void()> remove;                  // Remove button on a stale section
		std::function<void(bool collapsed)> collapsed; // header clicked
	};

	OwnerSection(const OwnerSummary &owner, const nlohmann::json &commands, const nlohmann::json &dock,
		     const nlohmann::json &state, bool collapsed, Callbacks callbacks, QWidget *parent = nullptr);

	const std::string &owner() const { return owner_; }
	std::uint64_t generation() const { return generation_; }
	bool stale() const { return stale_; }

	void set_stale(bool stale);
	// changes: the registry's change set (deleted keys are null)
	void apply_changes(const nlohmann::json &changes);

protected:
	void changeEvent(QEvent *event) override;

private:
	void apply_large_style(QLabel *label);
	QWidget *build_control(const nlohmann::json &control);
	QWidget *with_label(const nlohmann::json &control, QWidget *input);
	void run_button(const nlohmann::json &button);
	void send(const std::string &command, const nlohmann::json &args);
	void show_error(const QString &text);
	void refresh_bound(const std::string &key);
	void set_collapsed(bool collapsed);

	std::string owner_;
	std::uint64_t generation_ = 0;
	bool stale_ = false;
	bool collapsed_ = false;
	Callbacks callbacks_;

	nlohmann::json state_ = nlohmann::json::object();
	std::map<std::string, nlohmann::json> commands_; // id -> command (from Registry::get_commands)

	QToolButton *header_ = nullptr;
	QLabel *stale_label_ = nullptr;
	QPushButton *remove_button_ = nullptr;
	QWidget *content_ = nullptr;
	QLabel *status_ = nullptr;

	std::multimap<std::string, QLabel *> labels_;     // state key -> bound labels
	std::multimap<std::string, QCheckBox *> toggles_; // state key -> bound toggles
	struct BoundButton {
		QPushButton *button;
		std::string command_label; // shown while the bound key is not set
	};
	std::multimap<std::string, BoundButton> buttons_; // label_bind key -> buttons
	std::map<std::string, QWidget *> inputs_;         // number/text control id -> widget
	std::vector<QLabel *> large_labels_;              // style "large"; resized on theme change
};

} // namespace luabridge::dock
