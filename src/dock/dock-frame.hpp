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
// closed anything.
//
// So until end_layout_restore(), an event filter on the dock's frame keeps
// close events from reaching OBS's handler. Only on OBS's own frame (see
// obs_dock_frame; otherwise nothing is installed, and that is logged). The
// event stays accepted: the dock is still hidden as the layout says, only the
// notice is skipped.
//
// A close by the user is never swallowed: the guard ends at
// OBS_FRONTEND_EVENT_FINISHED_LOADING, which OBS sends at the end of its
// startup (OBSBasic::OBSInit), before its event loop runs and handles any
// click. After that every close, and OBS's notice for it, is untouched.
//
// Call right after adding a dock; does nothing once the restore is over.
void guard_layout_restore(QWidget *content);
// At OBS_FRONTEND_EVENT_FINISHED_LOADING (the restore is over), and at exit in
// case OBS never finished loading. Removes the filter from every frame.
void end_layout_restore();

} // namespace luabridge::dock
