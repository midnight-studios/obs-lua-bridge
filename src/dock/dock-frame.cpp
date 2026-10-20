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

#include "dock-frame.hpp"

#include <QDockWidget>
#include <QEvent>

#include <obs-module.h>
#include <plugin-support.h>

namespace luabridge::dock {

namespace {

bool restore_over = false;

class RestoreGuard : public QObject {
public:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		// The filter is deleted when the restore is over; the check is a second
		// line of defence, so a close by the user always reaches OBS
		if (event->type() != QEvent::Close || restore_over)
			return false;
		// Not delivered, so OBS shows no notice; the event stays accepted, so
		// Qt still hides the dock as the saved layout says
		obs_log(LOG_INFO, "dock '%s': ignored a close event while OBS restored the dock layout",
			watched->objectName().toUtf8().constData());
		return true;
	}
};

RestoreGuard *guard = nullptr;

} // namespace

QDockWidget *obs_dock_frame(QWidget *content)
{
	auto *frame = content ? qobject_cast<QDockWidget *>(content->parentWidget()) : nullptr;
	return frame && frame->inherits("OBSDock") ? frame : nullptr;
}

void guard_layout_restore(QWidget *content)
{
	if (restore_over)
		return;
	QDockWidget *frame = obs_dock_frame(content);
	if (!frame) {
		// Not OBS's dock frame: leave its events alone
		obs_log(LOG_INFO, "dock: no OBS dock frame found; OBS may show its close notice at startup");
		return;
	}
	if (!guard)
		guard = new RestoreGuard;
	frame->installEventFilter(guard);
}

void end_layout_restore()
{
	restore_over = true;
	// Deleting the filter removes it from every frame it was installed on
	delete guard;
	guard = nullptr;
}

} // namespace luabridge::dock
