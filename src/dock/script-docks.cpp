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

#include "script-docks.hpp"

#include <QDockWidget>
#include <QRect>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/bmem.h>
#include <util/platform.h>

#include "dock-events.hpp"
#include "lua-bridge-dock.hpp"

namespace luabridge::dock {

namespace {

constexpr const char *config_file = "docks.json";
constexpr int new_dock_width = 340;
constexpr int new_dock_height = 420;

std::string module_config_path(const char *file)
{
	char *path = obs_module_config_path(file);
	std::string out = path ? path : "";
	bfree(path);
	return out;
}

QString qstr(const std::string &s)
{
	return QString::fromUtf8(s.data(), static_cast<int>(s.size()));
}

// The QDockWidget OBS wrapped our widget in
QDockWidget *dock_of(QWidget *widget)
{
	return widget ? qobject_cast<QDockWidget *>(widget->parentWidget()) : nullptr;
}

} // namespace

ScriptDocks::ScriptDocks(Registry &registry, Emitter &emitter, EventSink &events)
	: registry_(registry),
	  emitter_(emitter),
	  events_(events)
{
}

void ScriptDocks::load()
{
	path_ = module_config_path(config_file);
	if (path_.empty())
		return;

	char *text = os_quick_read_utf8_file(path_.c_str());
	if (!text)
		return; // no script docks chosen yet
	script_docks::ParseResult parsed = script_docks::parse_config(text);
	bfree(text);

	for (const auto &warning : parsed.warnings)
		obs_log(LOG_WARNING, "%s: %s", config_file, warning.c_str());
	if (!parsed.ok) {
		// Keep the file for the user to look at; the next save starts a new one
		std::string kept = path_ + ".invalid";
		bool renamed = os_rename(path_.c_str(), kept.c_str()) == 0;
		obs_log(LOG_WARNING, "%s: %s; starting without script docks%s", config_file, parsed.error.c_str(),
			renamed ? " (the file was renamed to docks.json.invalid)" : "");
		return;
	}

	config_ = std::move(parsed.config);
	for (const auto &dock : config_.docks)
		create(dock);
	if (!config_.docks.empty())
		obs_log(LOG_INFO, "created %zu script dock(s)", widgets_.size());
}

QString ScriptDocks::add(const std::string &owner, const std::string &display_name)
{
	switch (script_docks::add_dock(config_, owner, display_name)) {
	case script_docks::AddResult::added:
		break;
	case script_docks::AddResult::already_added:
		return {};
	case script_docks::AddResult::limit_reached:
		return QString::fromUtf8(obs_module_text("LuaBridge.Docks.Limit"))
			.arg(static_cast<int>(script_docks::max_docks));
	case script_docks::AddResult::invalid_owner:
		return QString::fromUtf8(obs_module_text("LuaBridge.Docks.AddFailed"));
	}

	if (!create(*script_docks::find_dock(config_, owner))) {
		script_docks::remove_dock(config_, owner);
		return QString::fromUtf8(obs_module_text("LuaBridge.Docks.AddFailed"));
	}
	save();
	obs_log(LOG_INFO, "added a script dock for '%s'", printable(owner).c_str());

	// OBS adds docks hidden, and restores saved layouts only at startup: open the
	// new dock as a floating window over the main window
	if (QDockWidget *dock = dock_of(widgets_[owner])) {
		auto *main = static_cast<QWidget *>(obs_frontend_get_main_window());
		dock->setFloating(true);
		dock->show();
		QRect area = main ? main->geometry() : QRect(0, 0, new_dock_width * 2, new_dock_height * 2);
		QRect rect(0, 0, new_dock_width, new_dock_height);
		rect.moveCenter(area.center());
		dock->setGeometry(rect);
		dock->raise();
	}
	return {};
}

void ScriptDocks::remove(const std::string &owner)
{
	destroy(owner);
	if (script_docks::remove_dock(config_, owner)) {
		save();
		obs_log(LOG_INFO, "removed the script dock for '%s'", printable(owner).c_str());
	}
}

void ScriptDocks::set_title(const std::string &owner, const std::string &title)
{
	script_docks::ScriptDock *dock = script_docks::find_dock(config_, owner);
	if (!dock)
		return;
	std::string cleaned = script_docks::clean_title(title);
	if (cleaned == dock->title)
		return;
	dock->title = std::move(cleaned);
	save();
	apply_title(owner);
}

void ScriptDocks::remove_all()
{
	while (!widgets_.empty())
		destroy(widgets_.begin()->first);
}

bool ScriptDocks::create(const script_docks::ScriptDock &dock)
{
	auto *widget = new LuaBridgeDock(registry_, emitter_, events_, dock.owner);
	std::string id = script_docks::dock_id(dock.owner);
	if (!obs_frontend_add_dock_by_id(id.c_str(), script_docks::dock_title(dock).c_str(), widget)) {
		obs_log(LOG_WARNING, "could not add the script dock '%s'", printable(id).c_str());
		delete widget;
		return false;
	}
	widgets_[dock.owner] = widget;
	if (seen_.count(dock.owner))
		widget->set_owner_seen();
	widget->on_display_name(
		[this, owner = dock.owner](const std::string &name) { display_name_seen(owner, name); });
	attach_events(widget);
	return true;
}

void ScriptDocks::destroy(const std::string &owner)
{
	auto it = widgets_.find(owner);
	if (it == widgets_.end())
		return;
	QPointer<LuaBridgeDock> widget = it->second;
	widgets_.erase(it);
	if (!widget)
		return;
	detach_events(widget);
	widget->on_display_name({}); // nothing may call back into this object once removed
	// An open confirm must not run its command (or outlive its parent)
	widget->close_dialogs();
	// Deletes the dock and our widget
	obs_frontend_remove_dock(script_docks::dock_id(owner).c_str());
}

void ScriptDocks::apply_title(const std::string &owner)
{
	const script_docks::ScriptDock *dock = script_docks::find_dock(config_, owner);
	auto it = widgets_.find(owner);
	if (!dock || it == widgets_.end())
		return;
	// The Docks menu entry follows the dock's window title
	if (QDockWidget *dock_widget = dock_of(it->second))
		dock_widget->setWindowTitle(qstr(script_docks::dock_title(*dock)));
}

void ScriptDocks::display_name_seen(const std::string &owner, const std::string &display_name)
{
	seen_.insert(owner);
	if (!script_docks::note_display_name(config_, owner, display_name))
		return;
	save();
	apply_title(owner);
}

void ScriptDocks::save()
{
	if (path_.empty())
		return;
	std::string dir = module_config_path("");
	if (!dir.empty() && os_mkdirs(dir.c_str()) == MKDIR_ERROR) {
		obs_log(LOG_WARNING, "could not create %s", dir.c_str());
		return;
	}
	std::string text = script_docks::dump_config(config_);
	if (!os_quick_write_utf8_file_safe(path_.c_str(), text.data(), text.size(), false, "tmp", "bak"))
		obs_log(LOG_WARNING, "could not save %s", config_file);
}

} // namespace luabridge::dock
