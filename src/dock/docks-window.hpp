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

// The "Lua Bridge Docks" window (Tools > Lua Bridge): every registered owner and
// every owner with a dock, with a checkbox to give it a dock of its own and a
// custom dock title. Non-modal, so it never blocks OBS from closing. UI thread
// only.

#include <string>
#include <vector>

#include <QDialog>

#include "registry.hpp"

class QLabel;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;
class QTimer;

namespace luabridge::dock {

class ScriptDocks;

class DocksWindow : public QDialog {
public:
	DocksWindow(ScriptDocks &docks, Registry &registry, QWidget *parent);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;

private:
	void refresh();
	void rebuild();
	void set_docked(const std::string &owner, const std::string &display_name, bool docked);
	void item_changed(QTableWidgetItem *item);
	void show_message(const QString &message);

	ScriptDocks &docks_;
	Registry &registry_;

	QStackedWidget *stack_ = nullptr;
	QTableWidget *table_ = nullptr;
	QLabel *message_ = nullptr;
	QTimer *timer_ = nullptr;

	// What the table shows: owner and docked, per row; a change rebuilds it
	std::vector<std::pair<std::string, bool>> shown_;
	bool built_ = false; // the first refresh always builds (the table may stay empty)
};

} // namespace luabridge::dock
