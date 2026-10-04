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

#include "lua-bridge-dock.hpp"

#include <algorithm>
#include <vector>

#include <QLabel>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <obs-module.h>
#include <plugin-support.h>

#include "dock-logic.hpp"
#include "owner-section.hpp"
#include "state-events.hpp"

namespace luabridge::dock {

using json = nlohmann::json;

namespace {
constexpr int stale_check_interval_ms = 1000;
}

LuaBridgeDock::LuaBridgeDock(Registry &registry, Emitter &emitter, EventSink &events, QWidget *parent)
	: QWidget(parent),
	  registry_(registry),
	  emitter_(emitter),
	  events_(events)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	stack_ = new QStackedWidget(this);
	layout->addWidget(stack_);

	auto *placeholder = new QLabel(QString::fromUtf8(obs_module_text("LuaBridge.Dock.Placeholder")), this);
	placeholder->setWordWrap(true);
	placeholder->setAlignment(Qt::AlignCenter);
	placeholder->setContentsMargins(12, 12, 12, 12);
	stack_->addWidget(placeholder);

	scroll_ = new QScrollArea(this);
	scroll_->setWidgetResizable(true);
	scroll_->setFrameShape(QFrame::NoFrame);
	auto *list = new QWidget(scroll_);
	sections_layout_ = new QVBoxLayout(list);
	sections_layout_->setContentsMargins(4, 4, 4, 4);
	sections_layout_->setSpacing(6);
	sections_layout_->addStretch(1);
	scroll_->setWidget(list);
	stack_->addWidget(scroll_);

	// Staleness is time-based, so no event announces it: check every second
	auto *stale_timer = new QTimer(this);
	connect(stale_timer, &QTimer::timeout, this, [this] { schedule_reconcile(); });
	stale_timer->start(stale_check_interval_ms);

	schedule_reconcile();
}

void LuaBridgeDock::schedule_reconcile()
{
	if (reconcile_pending_)
		return;
	reconcile_pending_ = true;
	QTimer::singleShot(0, this, [this] {
		reconcile_pending_ = false;
		reconcile();
	});
}

void LuaBridgeDock::reconcile()
{
	std::vector<dock_logic::ShownSection> shown;
	for (const auto &[owner, section] : sections_)
		shown.push_back({owner, section->generation(), section->stale()});

	dock_logic::SectionPlan plan = dock_logic::plan_sections(shown, registry_.list_owners());

	for (const auto &owner : plan.remove) {
		OwnerSection *section = sections_[owner];
		sections_.erase(owner);
		sections_layout_->removeWidget(section);
		section->deleteLater();
	}

	for (const auto &owner : plan.rebuild) {
		OwnerSection *old_section = sections_[owner.id];
		OwnerSection *new_section = build_section(owner);
		if (!new_section)
			continue; // unregistered meanwhile; the next reconcile removes it
		int index = sections_layout_->indexOf(old_section);
		sections_layout_->removeWidget(old_section);
		old_section->deleteLater();
		sections_layout_->insertWidget(index, new_section); // same position as before
		sections_[owner.id] = new_section;
	}

	for (const auto &owner : plan.create) {
		OwnerSection *section = build_section(owner);
		if (!section)
			continue;
		sections_layout_->insertWidget(sections_layout_->count() - 1, section); // before the stretch
		sections_[owner.id] = section;
	}

	for (const auto &[owner, stale] : plan.stale_changes)
		sections_[owner]->set_stale(stale);

	update_placeholder();
}

OwnerSection *LuaBridgeDock::build_section(const OwnerSummary &owner)
{
	json commands, controls, state;
	if (!registry_.get_commands(owner.id, commands).ok || !registry_.get_dock(owner.id, controls).ok ||
	    !registry_.get_state(owner.id, state).ok)
		return nullptr;

	OwnerSection::Callbacks callbacks;
	callbacks.send = [this, id = owner.id](const std::string &command, const json &args) {
		return send_command(id, command, args);
	};
	callbacks.remove = [this, id = owner.id] {
		remove_owner(id);
	};
	callbacks.collapsed = [this, id = owner.id](bool collapsed) {
		collapsed_[id] = collapsed;
	};

	return new OwnerSection(owner, commands, controls, state, collapsed_[owner.id], std::move(callbacks),
				sections_layout_->parentWidget());
}

std::string LuaBridgeDock::send_command(const std::string &owner, const std::string &command, const json &args)
{
	std::string args_json;
	Result r = registry_.check_command(owner, command, args.dump(), &args_json);
	if (!r.ok) {
		obs_log(LOG_WARNING, "dock %s(%s): %s", command.c_str(), owner.c_str(), r.error.c_str());
		return r.error;
	}
	emitter_.command(owner, command, args_json, "dock");
	return {};
}

void LuaBridgeDock::remove_owner(const std::string &owner)
{
	bool removed = false;
	unregister_and_notify(registry_, owner, events_, &removed);
	if (removed)
		obs_log(LOG_INFO, "removed stale owner '%s' from the dock", owner.c_str());
	schedule_reconcile();
}

void LuaBridgeDock::update_placeholder()
{
	stack_->setCurrentWidget(sections_.empty() ? stack_->widget(0) : scroll_);
}

void LuaBridgeDock::close_dialogs()
{
	for (const auto &[owner, section] : sections_)
		section->close_dialogs();
}

void LuaBridgeDock::apply_state_changes(const std::string &owner, const std::string &changes_json)
{
	auto it = sections_.find(owner);
	if (it == sections_.end())
		return; // a section built later reads the full state
	try {
		it->second->apply_changes(json::parse(changes_json));
	} catch (const std::exception &e) {
		obs_log(LOG_ERROR, "dock: could not apply state change: %s", e.what());
	}
}

} // namespace luabridge::dock
