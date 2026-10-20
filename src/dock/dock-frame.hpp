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

// The dock frame OBS wraps a dock's widget in (obs_frontend_add_dock_by_id),
// and what the plugin does with it. UI thread only.

class QDockWidget;
class QWidget;

namespace luabridge::dock {

// The frame of a widget added with obs_frontend_add_dock_by_id: OBS's own
// QDockWidget subclass (OBSDock). nullptr if the parent is anything else (a
// future OBS may wrap docks differently), so callers fall back safely.
QDockWidget *obs_dock_frame(QWidget *content);

// At startup OBS restores the saved dock layout (QMainWindow::restoreState).
// With Qt 6.11 (OBS 32.2 and later), the restore sends a close event to a dock
// that was docked in the main window and closed with its X, and OBS answers
// every close event with its "Closing Dockable Window" notice, although nobody
// closed anything. Until end_layout_restore(), our docks' frames ignore close
// events: the dock is still hidden as the layout says, without the notice.
// Call right after adding a dock; does nothing once the restore is over.
void guard_layout_restore(QWidget *content);
// At OBS_FRONTEND_EVENT_FINISHED_LOADING (the restore is over), or at exit
void end_layout_restore();

} // namespace luabridge::dock
