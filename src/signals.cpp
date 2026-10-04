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

#include "signals.hpp"

#include <atomic>
#include <functional>
#include <mutex>

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QThread>

#include <obs.h>
#include <plugin-support.h>

namespace luabridge::signaling {

namespace {

std::atomic<bool> shutting_down{true}; // until start()

// Queued emits are posted to this object; deleting it discards those still pending
std::mutex context_mutex;
QObject *context = nullptr;

void post(std::function<void()> fn)
{
	if (shutting_down)
		return;

	// The lock keeps stop() from deleting the context while we post to it
	std::lock_guard lock(context_mutex);
	if (!context)
		return;
	QMetaObject::invokeMethod(
		context,
		[fn = std::move(fn)] {
			if (shutting_down)
				return;
			try {
				fn();
			} catch (...) {
				obs_log(LOG_ERROR, "unexpected exception while emitting a signal");
			}
		},
		Qt::QueuedConnection);
}

void signal(const char *name, calldata_t *cd)
{
	signal_handler_signal(obs_get_signal_handler(), name, cd);
}

class QtEmitter final : public Emitter {
public:
	void command(std::string owner, std::string command, std::string json, std::string origin) override
	{
		post([owner = std::move(owner), command = std::move(command), json = std::move(json),
		      origin = std::move(origin)] {
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_string(&cd, "owner", owner.c_str());
			calldata_set_string(&cd, "command", command.c_str());
			calldata_set_string(&cd, "json", json.c_str());
			calldata_set_string(&cd, "origin", origin.c_str());
			signal("luabridge_command", &cd);
			calldata_free(&cd);
		});
	}

	void event(std::string owner, std::string event, std::string json) override
	{
		post([owner = std::move(owner), event = std::move(event), json = std::move(json)] {
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_string(&cd, "owner", owner.c_str());
			calldata_set_string(&cd, "event", event.c_str());
			calldata_set_string(&cd, "json", json.c_str());
			signal("luabridge_event", &cd);
			calldata_free(&cd);
		});
	}

	void ready(std::string json) override
	{
		post([json = std::move(json)] {
			calldata_t cd;
			calldata_init(&cd);
			calldata_set_string(&cd, "json", json.c_str());
			signal("luabridge_ready", &cd);
			calldata_free(&cd);
		});
	}
};

} // namespace

void declare()
{
	static const char *decls[] = {
		"void luabridge_command(string owner, string command, string json, string origin)",
		"void luabridge_event(string owner, string event, string json)",
		"void luabridge_ready(string json)",
		nullptr,
	};
	signal_handler_add_array(obs_get_signal_handler(), decls);
}

void start()
{
	std::lock_guard lock(context_mutex);
	if (!context) {
		context = new QObject();
		// Queued emits run in the object's thread, which must be the UI thread
		if (QCoreApplication *app = QCoreApplication::instance())
			context->moveToThread(app->thread());
	}
	shutting_down = false;
}

void begin_shutdown()
{
	shutting_down = true;
}

void stop()
{
	begin_shutdown();

	QObject *old = nullptr;
	{
		std::lock_guard lock(context_mutex);
		old = context;
		context = nullptr;
	}
	if (!old)
		return;
	// ~QObject removes the events still posted to it, so pending emits never run
	if (old->thread() == QThread::currentThread())
		delete old;
	else
		old->deleteLater();
}

Emitter &emitter()
{
	static QtEmitter instance;
	return instance;
}

} // namespace luabridge::signaling
