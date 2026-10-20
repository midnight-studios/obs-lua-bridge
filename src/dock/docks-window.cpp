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

#include "docks-window.hpp"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <obs-module.h>

#include "script-docks-config.hpp"
#include "script-docks.hpp"

namespace luabridge::dock {

namespace {

constexpr int refresh_interval_ms = 1000; // staleness is time-based

enum Column { col_script, col_owner, col_status, col_dock, col_title, column_count };

// The owner id is kept on each row's Dock item
constexpr int owner_role = Qt::UserRole;

QString text(const char *locale_key)
{
	return QString::fromUtf8(obs_module_text(locale_key));
}

QString qstr(const std::string &s)
{
	return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

QString status_text(script_docks::OwnerStatus status)
{
	switch (status) {
	case script_docks::OwnerStatus::running:
		return text("LuaBridge.Docks.Status.Running");
	case script_docks::OwnerStatus::not_responding:
		return text("LuaBridge.Docks.Status.NotResponding");
	case script_docks::OwnerStatus::not_loaded:
		break;
	}
	return text("LuaBridge.Docks.Status.NotLoaded");
}

QTableWidgetItem *read_only_item(const QString &value)
{
	auto *item = new QTableWidgetItem(value);
	item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
	return item;
}

} // namespace

DocksWindow::DocksWindow(ScriptDocks &docks, Registry &registry, QWidget *parent)
	: QDialog(parent),
	  docks_(docks),
	  registry_(registry)
{
	setWindowTitle(text("LuaBridge.Docks.WindowTitle"));
	// Non-modal: a modal window disables OBS's main window and blocks closing OBS
	setWindowModality(Qt::NonModal);
	resize(720, 380);

	auto *layout = new QVBoxLayout(this);

	auto *intro = new QLabel(text("LuaBridge.Docks.Intro"), this);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	// Plain checkable and editable items, not widgets in cells: Qt exposes items
	// to screen readers and UI automation, cell widgets it doesn't
	stack_ = new QStackedWidget(this);
	table_ = new QTableWidget(0, column_count, this);
	table_->setHorizontalHeaderLabels({text("LuaBridge.Docks.Column.Script"), text("LuaBridge.Docks.Column.Owner"),
					   text("LuaBridge.Docks.Column.Status"), text("LuaBridge.Docks.Column.Dock"),
					   text("LuaBridge.Docks.Column.Title")});
	table_->verticalHeader()->hide();
	table_->setSelectionMode(QAbstractItemView::SingleSelection);
	table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
				QAbstractItemView::AnyKeyPressed | QAbstractItemView::SelectedClicked);
	QHeaderView *header = table_->horizontalHeader();
	header->setSectionResizeMode(col_script, QHeaderView::Stretch);
	header->setSectionResizeMode(col_owner, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(col_status, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(col_dock, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(col_title, QHeaderView::Stretch);
	connect(table_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) { item_changed(item); });
	stack_->addWidget(table_);

	auto *empty = new QLabel(text("LuaBridge.Docks.Empty"), this);
	empty->setWordWrap(true);
	empty->setAlignment(Qt::AlignCenter);
	stack_->addWidget(empty);
	layout->addWidget(stack_, 1);

	message_ = new QLabel(this);
	message_->setWordWrap(true);
	message_->setStyleSheet(QStringLiteral("color: #e5534b;"));
	message_->hide();
	layout->addWidget(message_);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	timer_ = new QTimer(this);
	connect(timer_, &QTimer::timeout, this, [this] { refresh(); });
}

void DocksWindow::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	refresh();
	timer_->start(refresh_interval_ms);
}

void DocksWindow::hideEvent(QHideEvent *event)
{
	timer_->stop();
	QDialog::hideEvent(event);
}

void DocksWindow::refresh()
{
	// While a title is being edited (its editor, a child of the table, has focus),
	// a rebuild would lose the typed text; try again on the next tick
	if (QWidget *focus = QApplication::focusWidget(); focus && table_->isAncestorOf(focus) && focus != table_)
		return;
	auto rows = script_docks::manager_rows(docks_.config(), registry_.list_owners());

	std::vector<std::pair<std::string, bool>> keys;
	for (const auto &row : rows)
		keys.emplace_back(row.owner, row.docked);
	if (!built_ || keys != shown_) {
		rebuild();
		return;
	}
	// Same rows: update names and statuses in place, so a title being typed isn't lost
	QSignalBlocker block(table_);
	for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
		table_->item(i, col_script)->setText(qstr(rows[i].display_name));
		table_->item(i, col_status)->setText(status_text(rows[i].status));
	}
}

void DocksWindow::rebuild()
{
	auto rows = script_docks::manager_rows(docks_.config(), registry_.list_owners());
	QSignalBlocker block(table_);
	built_ = true;
	shown_.clear();
	table_->setRowCount(0);
	table_->setRowCount(static_cast<int>(rows.size()));

	for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
		const auto &row = rows[i];
		shown_.emplace_back(row.owner, row.docked);

		table_->setItem(i, col_script, read_only_item(qstr(row.display_name)));
		table_->setItem(i, col_owner, read_only_item(qstr(row.owner)));
		table_->setItem(i, col_status, read_only_item(status_text(row.status)));

		auto *dock = new QTableWidgetItem(text("LuaBridge.Docks.Column.Dock"));
		dock->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
		dock->setCheckState(row.docked ? Qt::Checked : Qt::Unchecked);
		dock->setToolTip(text("LuaBridge.Docks.DockTooltip"));
		dock->setData(owner_role, qstr(row.owner));
		table_->setItem(i, col_dock, dock);

		// Editable only while the script has a dock
		auto *title = new QTableWidgetItem(qstr(row.title));
		Qt::ItemFlags flags = Qt::ItemIsSelectable;
		if (row.docked)
			flags |= Qt::ItemIsEnabled | Qt::ItemIsEditable;
		title->setFlags(flags);
		title->setToolTip(text("LuaBridge.Docks.TitleTooltip"));
		table_->setItem(i, col_title, title);
	}
	stack_->setCurrentIndex(rows.empty() ? 1 : 0);
}

void DocksWindow::item_changed(QTableWidgetItem *item)
{
	int row = item->row();
	QTableWidgetItem *dock = table_->item(row, col_dock);
	if (!dock)
		return;
	std::string owner = dock->data(owner_role).toString().toStdString();

	if (item->column() == col_dock) {
		set_docked(owner, table_->item(row, col_script)->text().toStdString(),
			   item->checkState() == Qt::Checked);
	} else if (item->column() == col_title) {
		docks_.set_title(owner, item->text().toStdString());
		// Show the title as saved (trimmed, control characters removed)
		if (const auto *saved = script_docks::find_dock(docks_.config(), owner)) {
			QSignalBlocker block(table_);
			item->setText(qstr(saved->title));
		}
	}
}

void DocksWindow::set_docked(const std::string &owner, const std::string &display_name, bool docked)
{
	if (docked) {
		QString error = docks_.add(owner, display_name);
		if (!error.isEmpty())
			show_message(error);
		else
			message_->hide();
	} else {
		docks_.remove(owner);
		message_->hide();
	}
	// Rebuild after this signal returns, so the row shows what was saved (a refused
	// tick is reverted)
	QTimer::singleShot(0, this, [this] { rebuild(); });
}

void DocksWindow::show_message(const QString &message)
{
	message_->setText(message);
	message_->show();
}

} // namespace luabridge::dock
