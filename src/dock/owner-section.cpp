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

#include "owner-section.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <obs-module.h>

#include "dock-logic.hpp"

namespace luabridge::dock {

using json = nlohmann::json;

namespace {

QString qstr(const std::string &s)
{
	return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

QString text(const char *locale_key)
{
	return QString::fromUtf8(obs_module_text(locale_key));
}

std::string string_field(const json &obj, const char *key)
{
	auto it = obj.find(key);
	return (it != obj.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

double number_field(const json &obj, const char *key, double fallback)
{
	auto it = obj.find(key);
	return (it != obj.end() && it->is_number()) ? it->get<double>() : fallback;
}

constexpr double default_number_range = 1000000.0;
constexpr double large_label_scale = 1.6;

} // namespace

OwnerSection::OwnerSection(const OwnerSummary &owner, const json &commands, const json &dock, const json &state,
			   bool collapsed, Callbacks callbacks, QWidget *parent)
	: QFrame(parent),
	  owner_(owner.id),
	  generation_(owner.generation),
	  callbacks_(std::move(callbacks)),
	  state_(state.is_object() ? state : json::object())
{
	for (const auto &cmd : commands)
		commands_[string_field(cmd, "id")] = cmd;

	setFrameShape(QFrame::StyledPanel);
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(6, 4, 6, 6);
	outer->setSpacing(4);

	// Header: collapse arrow + display name, stale marker, Remove (stale only)
	auto *header_row = new QHBoxLayout();
	header_ = new QToolButton(this);
	header_->setText(qstr(owner.display_name));
	header_->setToolTip(qstr(owner.id));
	header_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	header_->setAutoRaise(true);
	QFont header_font = header_->font();
	header_font.setBold(true);
	header_->setFont(header_font);
	connect(header_, &QToolButton::clicked, this, [this] {
		bool collapse = !collapsed_;
		set_collapsed(collapse);
		if (callbacks_.collapsed)
			callbacks_.collapsed(collapse);
	});
	header_row->addWidget(header_);

	stale_label_ = new QLabel(QStringLiteral("— ") + text("LuaBridge.Dock.NotResponding"), this);
	stale_label_->setEnabled(false); // drawn in the palette's disabled (dimmed) colour
	header_row->addWidget(stale_label_);
	header_row->addStretch(1);

	remove_button_ = new QPushButton(text("LuaBridge.Dock.Remove"), this);
	remove_button_->setToolTip(text("LuaBridge.Dock.RemoveTooltip"));
	connect(remove_button_, &QPushButton::clicked, this, [this] {
		if (callbacks_.remove)
			callbacks_.remove();
	});
	header_row->addWidget(remove_button_);
	outer->addLayout(header_row);

	// Controls
	content_ = new QWidget(this);
	auto *content_layout = new QVBoxLayout(content_);
	content_layout->setContentsMargins(4, 0, 0, 0);
	content_layout->setSpacing(4);
	for (const auto &control : dock) {
		if (QWidget *w = build_control(control))
			content_layout->addWidget(w);
	}
	outer->addWidget(content_);

	// Last command error, shown for a few seconds
	status_ = new QLabel(this);
	status_->setWordWrap(true);
	status_->setStyleSheet(QStringLiteral("color: #e5534b;"));
	status_->hide();
	outer->addWidget(status_);

	for (const auto &[key, value] : state_.items())
		refresh_bound(key);
	set_collapsed(collapsed);
	set_stale(owner.stale);
}

QWidget *OwnerSection::with_label(const json &control, QWidget *input)
{
	std::string label = string_field(control, "label");
	if (label.empty())
		return input;
	auto *row = new QWidget(this);
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(new QLabel(qstr(label), row));
	layout->addWidget(input, 1);
	return row;
}

QWidget *OwnerSection::build_control(const json &control)
{
	const std::string type = string_field(control, "type");

	if (type == "label") {
		auto *label = new QLabel(this);
		label->setWordWrap(true);
		label->setTextInteractionFlags(Qt::TextSelectableByMouse);
		if (string_field(control, "style") == "large") {
			large_labels_.push_back(label);
			apply_large_style(label);
		}
		std::string key = string_field(control, "bind");
		label->setText(qstr(dock_logic::format_state_value(nullptr)));
		labels_.emplace(key, label);
		return label;
	}

	if (type == "button") {
		std::string command = string_field(control, "command");
		const json &cmd = commands_[command];
		auto *button = new QPushButton(qstr(string_field(cmd, "label")), this);
		button->setToolTip(qstr(string_field(cmd, "description")));
		std::string label_bind = string_field(control, "label_bind");
		if (!label_bind.empty())
			buttons_.emplace(label_bind, BoundButton{button, string_field(cmd, "label")});
		connect(button, &QPushButton::clicked, this, [this, control] { run_button(control); });
		return button;
	}

	if (type == "row") {
		auto *row = new QWidget(this);
		auto *layout = new QHBoxLayout(row);
		layout->setContentsMargins(0, 0, 0, 0);
		auto items = control.find("items");
		if (items != control.end()) {
			for (const auto &item : *items) {
				if (QWidget *w = build_control(item))
					layout->addWidget(w, 1); // buttons share the width equally
			}
		}
		return row;
	}

	if (type == "number") {
		std::string id = string_field(control, "id");
		double min = number_field(control, "min", -default_number_range);
		double max = number_field(control, "max", default_number_range);
		double value = number_field(control, "default", 0.0);
		QWidget *input = nullptr;
		if (dock_logic::number_is_integer(control)) {
			auto *spin = new QSpinBox(this);
			spin->setRange(static_cast<int>(min), static_cast<int>(max));
			spin->setValue(static_cast<int>(value));
			input = spin;
		} else {
			auto *spin = new QDoubleSpinBox(this);
			spin->setDecimals(3);
			spin->setRange(min, max);
			spin->setValue(value);
			input = spin;
		}
		inputs_[id] = input;
		return with_label(control, input);
	}

	if (type == "text") {
		auto *edit = new QLineEdit(qstr(string_field(control, "default")), this);
		inputs_[string_field(control, "id")] = edit;
		return with_label(control, edit);
	}

	if (type == "toggle") {
		std::string command = string_field(control, "command");
		std::string key = string_field(control, "bind");
		auto *box = new QCheckBox(qstr(string_field(commands_[command], "label")), this);
		box->setToolTip(qstr(string_field(commands_[command], "description")));
		toggles_.emplace(key, box);
		connect(box, &QCheckBox::clicked, this, [this, command, key](bool checked) {
			// Show the script's state, not the click: revert until the script confirms
			refresh_bound(key);
			send(command, json{{"value", checked}});
		});
		return box;
	}

	if (type == "separator") {
		auto *line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		return line;
	}

	return nullptr; // registration already removed unknown types
}

void OwnerSection::run_button(const json &button)
{
	std::string command = string_field(button, "command");
	const json &cmd = commands_[command];

	auto confirm = cmd.find("confirm");
	if (confirm != cmd.end() && confirm->is_boolean() && confirm->get<bool>()) {
		QString question = text("LuaBridge.Dock.ConfirmText").arg(qstr(string_field(cmd, "label")));
		if (QMessageBox::question(this, text("LuaBridge.Dock.Title"), question) != QMessageBox::Yes)
			return;
	}

	std::map<std::string, json> values;
	for (const auto &[id, widget] : inputs_) {
		if (auto *spin = qobject_cast<QSpinBox *>(widget))
			values[id] = spin->value();
		else if (auto *dspin = qobject_cast<QDoubleSpinBox *>(widget))
			values[id] = dspin->value();
		else if (auto *edit = qobject_cast<QLineEdit *>(widget))
			values[id] = edit->text().toStdString();
	}
	send(command, dock_logic::build_button_args(button, cmd, values));
}

void OwnerSection::send(const std::string &command, const json &args)
{
	if (!callbacks_.send)
		return;
	std::string error = callbacks_.send(command, args);
	if (!error.empty())
		show_error(qstr(error));
}

void OwnerSection::show_error(const QString &message)
{
	status_->setText(message);
	status_->show();
	QTimer::singleShot(5000, status_, [status = status_, message] {
		if (status->text() == message)
			status->hide();
	});
}

void OwnerSection::refresh_bound(const std::string &key)
{
	auto value = state_.find(key);
	const json *v = value == state_.end() ? nullptr : &*value;

	auto labels = labels_.equal_range(key);
	for (auto it = labels.first; it != labels.second; ++it)
		it->second->setText(qstr(dock_logic::format_state_value(v)));

	auto buttons = buttons_.equal_range(key);
	for (auto it = buttons.first; it != buttons.second; ++it)
		it->second.button->setText(qstr(dock_logic::button_text(it->second.command_label, v)));

	auto toggles = toggles_.equal_range(key);
	for (auto it = toggles.first; it != toggles.second; ++it) {
		QSignalBlocker block(it->second);
		it->second->setChecked(v && v->is_boolean() && v->get<bool>());
	}
}

void OwnerSection::apply_changes(const json &changes)
{
	if (!changes.is_object())
		return;
	for (const auto &[key, value] : changes.items()) {
		if (value.is_null())
			state_.erase(key);
		else
			state_[key] = value;
		refresh_bound(key);
	}
}

// OBS themes set font-size on every QWidget in their stylesheet, which overrides
// setFont(). A widget's own stylesheet wins over the theme's, so the large size
// goes there, relative to the size the theme gives the label.
void OwnerSection::apply_large_style(QLabel *label)
{
	label->setStyleSheet(QString());
	label->ensurePolished();
	QFont font = label->font();
	double points = font.pointSizeF();
	if (points <= 0 && font.pixelSize() > 0)
		points = font.pixelSize() * 72.0 / label->logicalDpiY();
	if (points <= 0)
		points = 9.0;
	label->setStyleSheet(
		QStringLiteral("font-size: %1pt; font-weight: bold;").arg(points * large_label_scale, 0, 'f', 1));
}

void OwnerSection::changeEvent(QEvent *event)
{
	QFrame::changeEvent(event);
	// Theme switched: recompute the large size from the new theme's base size
	if (event->type() == QEvent::StyleChange) {
		for (QLabel *label : large_labels_)
			apply_large_style(label);
	}
}

void OwnerSection::set_stale(bool stale)
{
	stale_ = stale;
	content_->setEnabled(!stale);
	stale_label_->setVisible(stale);
	remove_button_->setVisible(stale);
}

void OwnerSection::set_collapsed(bool collapsed)
{
	collapsed_ = collapsed;
	content_->setVisible(!collapsed);
	header_->setArrowType(collapsed ? Qt::RightArrow : Qt::DownArrow);
}

} // namespace luabridge::dock
