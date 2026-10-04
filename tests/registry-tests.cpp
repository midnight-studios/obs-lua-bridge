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

// Unit tests for the registry (no OBS needed). Run with ctest or directly;
// exits non-zero if any check fails.

#include "dock-logic.hpp"
#include "event-data.hpp"
#include "fan-out-sink.hpp"
#include "log-limiter.hpp"
#include "rate-limit.hpp"
#include "state-events.hpp"
#include "registry.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace luabridge;
using namespace std::chrono_literals;

namespace {

int checks = 0;
int failures = 0;

void report(bool passed, const char *file, int line, const std::string &message)
{
	++checks;
	if (!passed) {
		++failures;
		std::fprintf(stderr, "%s:%d: %s\n", file, line, message.c_str());
	}
}

#define CHECK(cond) report((cond), __FILE__, __LINE__, "CHECK failed: " #cond)

#define CHECK_OK(expr)                                                                       \
	do {                                                                                 \
		Result r_ = (expr);                                                          \
		report(r_.ok, __FILE__, __LINE__, std::string("expected ok: " #expr " -> ") + r_.error); \
	} while (0)

#define CHECK_FAIL(expr, substr)                                                                 \
	do {                                                                                     \
		Result r_ = (expr);                                                              \
		report(!r_.ok && r_.error.find(substr) != std::string::npos, __FILE__, __LINE__, \
		       std::string("expected failure containing '") + (substr) +                \
			       "': " #expr " -> " + (r_.ok ? std::string("ok") : r_.error));     \
	} while (0)

// Registry with a controllable clock
struct Fixture {
	std::shared_ptr<Clock::time_point> now = std::make_shared<Clock::time_point>(Clock::time_point{} + 1h);
	Registry reg{[now = now] {
		return *now;
	}};

	void advance(Clock::duration d) { *now += d; }
};

std::string repeat(char c, std::size_t n)
{
	return std::string(n, c);
}

std::string with_commands(int count)
{
	std::string s = R"({"display_name":"T","commands":[)";
	for (int i = 0; i < count; ++i)
		s += (i ? "," : "") + std::string(R"({"id":"c)") + std::to_string(i) + "\"}";
	return s + "]}";
}

std::string with_dock(const std::string &dock)
{
	return R"({"display_name":"T","commands":[{"id":"go","args":{"n":"int"}},{"id":"flip","args":{"value":"bool"}}],"dock":)" +
	       dock + "}";
}

const char *b7_example = R"({
  "display_name": "Stopwatch",
  "commands": [
    { "id": "start",  "label": "Start",  "description": "Start the timer" },
    { "id": "pause",  "label": "Pause" },
    { "id": "reset",  "label": "Reset",  "confirm": true },
    { "id": "add",    "label": "+",      "args": { "seconds": "int" } }
  ],
  "dock": [
    { "type": "label",  "bind": "display",   "style": "large" },
    { "type": "row",    "items": [
        { "type": "button", "command": "start" },
        { "type": "button", "command": "pause" },
        { "type": "button", "command": "reset" } ] },
    { "type": "number", "id": "add_secs", "min": 1, "max": 3600, "default": 1 },
    { "type": "button", "command": "add", "args_from": { "seconds": "add_secs" } },
    { "type": "separator" }
  ]
})";

bool has_warning(const Result &r, const std::string &substr)
{
	for (const auto &w : r.warnings) {
		if (w.find(substr) != std::string::npos)
			return true;
	}
	return false;
}

void test_id_validation()
{
	CHECK(is_valid_owner_id("stopwatch"));
	CHECK(is_valid_owner_id("a.b-c_1"));
	CHECK(is_valid_owner_id("grumpydog.scoreboard"));
	CHECK(is_valid_owner_id(repeat('a', 64)));
	CHECK(!is_valid_owner_id(""));
	CHECK(!is_valid_owner_id(repeat('a', 65)));
	CHECK(!is_valid_owner_id("Stopwatch"));
	CHECK(!is_valid_owner_id("a b"));
	CHECK(!is_valid_owner_id("\xc3\xa9"));
	CHECK(!is_valid_owner_id("../x"));

	CHECK(is_valid_command_id("add_secs"));
	CHECK(is_valid_command_id(repeat('c', 64)));
	CHECK(!is_valid_command_id(repeat('c', 65)));
	CHECK(!is_valid_command_id("a.b"));
	CHECK(!is_valid_command_id("a-b"));
	CHECK(!is_valid_command_id("Start"));

	CHECK(is_valid_state_key("Display.Main_1"));
	CHECK(is_valid_state_key(repeat('K', 64)));
	CHECK(!is_valid_state_key(repeat('K', 65)));
	CHECK(!is_valid_state_key("a-b"));
	CHECK(!is_valid_state_key(""));

	CHECK(is_valid_event_name("score.changed"));
	CHECK(is_valid_event_name(repeat('e', 64)));
	CHECK(!is_valid_event_name(repeat('e', 65)));
	CHECK(!is_valid_event_name("bad event"));
}

void test_register()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("minimal", R"({"display_name":"Minimal"})"));

	Result r = f.reg.register_owner("stopwatch", b7_example);
	CHECK(r.ok);
	CHECK(r.warnings.empty());

	CHECK_FAIL(f.reg.register_owner("a", R"({})"), "display_name");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":""})"), "display_name");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":")" + repeat('x', 129) + "\"}"), "display_name");
	CHECK_OK(f.reg.register_owner("a", R"({"display_name":")" + repeat('x', 128) + "\"}"));
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":5})"), "display_name");
	CHECK_FAIL(f.reg.register_owner("Bad", R"({"display_name":"x"})"), "invalid owner id");
}

void test_json_size_and_parsing()
{
	Fixture f;
	std::string head = R"({"display_name":"x","pad":")";
	std::string tail = "\"}";
	std::string exact = head + repeat('a', limits::max_json_bytes - head.size() - tail.size()) + tail;
	CHECK(exact.size() == limits::max_json_bytes);
	CHECK_OK(f.reg.register_owner("big", exact));

	std::string over = head + repeat('a', limits::max_json_bytes + 1 - head.size() - tail.size()) + tail;
	CHECK(over.size() == limits::max_json_bytes + 1);
	CHECK_FAIL(f.reg.register_owner("big", over), "json exceeds 65536 bytes");

	CHECK_FAIL(f.reg.register_owner("a", "{not json"), "invalid JSON");
	CHECK_FAIL(f.reg.register_owner("a", "[1,2]"), "must be an object");
	CHECK_FAIL(f.reg.register_owner("a", "\"str\""), "must be an object");
	CHECK_FAIL(f.reg.register_owner("a", "{\"display_name\":\"\xff\xfe\"}"), "invalid JSON");
	CHECK_OK(f.reg.register_owner("a", "{\"display_name\":\"caf\xc3\xa9 \xf0\x9f\x8e\xae\"}"));
}

void test_command_limits()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("a", with_commands(64)));
	CHECK_FAIL(f.reg.register_owner("a", with_commands(65)), "too many commands (max 64)");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":"T","commands":[{"id":"x"},{"id":"x"}]})"),
		   "duplicate command 'x'");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":"T","commands":[{"id":"x","args":{"v":"float"}}]})"),
		   "must have type");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":"T","commands":[{"id":"Bad"}]})"), "commands[0]");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":"T","commands":[{"id":"x","confirm":"yes"}]})"),
		   "confirm");
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":"T","commands":{}})"), "commands must be an array");

	std::string args;
	for (int i = 0; i < 17; ++i)
		args += (i ? "," : "") + std::string("\"a") + std::to_string(i) + "\":\"int\"";
	CHECK_FAIL(f.reg.register_owner("a", R"({"display_name":"T","commands":[{"id":"x","args":{)" + args + "}}]}"),
		   "too many args (max 16)");
}

void test_owner_limit()
{
	Fixture f;
	for (std::size_t i = 0; i < limits::max_owners; ++i)
		CHECK_OK(f.reg.register_owner("o" + std::to_string(i), R"({"display_name":"x"})"));
	CHECK(f.reg.owner_count() == limits::max_owners);
	CHECK_FAIL(f.reg.register_owner("extra", R"({"display_name":"x"})"), "too many owners (max 128)");
	CHECK_OK(f.reg.register_owner("o0", R"({"display_name":"again"})"));
	CHECK_OK(f.reg.unregister_owner("o5"));
	CHECK_OK(f.reg.register_owner("extra", R"({"display_name":"x"})"));
	CHECK(f.reg.owner_count() == limits::max_owners);
}

void test_reregister()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("s", R"({"display_name":"S","commands":[{"id":"a"}]})"));
	CHECK_OK(f.reg.set_state("s", R"({"x":1})", nullptr));

	CHECK_OK(f.reg.register_owner("s", R"({"display_name":"S","commands":[{"id":"b"}]})"));
	CHECK_FAIL(f.reg.check_command("s", "a", ""), "unknown command 'a'");
	CHECK_OK(f.reg.check_command("s", "b", ""));
	std::string changes;
	CHECK_OK(f.reg.set_state("s", R"({"x":1})", &changes));
	CHECK(changes == R"({"x":1})"); // state was cleared by re-registering

	// A failed re-registration keeps the previous one
	CHECK_FAIL(f.reg.register_owner("s", "{broken"), "invalid JSON");
	CHECK_FAIL(f.reg.register_owner("s", with_commands(65)), "too many commands");
	CHECK_OK(f.reg.check_command("s", "b", ""));
	CHECK_OK(f.reg.set_state("s", R"({"x":1})", &changes));
	CHECK(changes == "{}");
}

void test_dock()
{
	Fixture f;
	Result r = f.reg.register_owner("d", with_dock(R"([{"type":"slider"},{"type":"separator"}])"));
	CHECK(r.ok);
	CHECK(r.warnings.size() == 1);
	CHECK(has_warning(r, "dock[0]: unknown control type 'slider'; skipped"));

	r = f.reg.register_owner("d", with_dock(R"([{"type":"button","command":"nope"}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "command 'nope' is not declared"));

	r = f.reg.register_owner("d", with_dock(R"([{"type":"row","items":[{"type":"row","items":[]}]}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "dock[0].items[0]: rows cannot be nested"));

	r = f.reg.register_owner("d", with_dock(R"([{"type":"number","id":"n","min":10,"max":1}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "min <= default <= max"));

	r = f.reg.register_owner("d", with_dock(R"([{"type":"label"}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "invalid or missing bind"));

	r = f.reg.register_owner("d", with_dock(R"([{"type":"button","command":"go","args_from":{"n":"missing"}}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "must name a number or text control"));

	// A fresh owner, so the only possible warnings come from the dock controls
	CHECK_OK(f.reg.unregister_owner("d"));
	r = f.reg.register_owner(
		"d", with_dock(R"([{"type":"number","id":"v"},{"type":"button","command":"go","args_from":{"n":"v"}},)"
			       R"({"type":"toggle","bind":"on","command":"flip"},{"type":"text","id":"t"}])"));
	CHECK(r.ok);
	CHECK(r.warnings.empty());

	r = f.reg.register_owner("d", with_dock(R"([{"type":"number","id":"v"},{"type":"text","id":"v"}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "duplicate id 'v'"));

	// A toggle sends {"value": bool}, so its command must declare that arg
	r = f.reg.register_owner("d", with_dock(R"([{"type":"toggle","bind":"on","command":"go"}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "toggle command 'go' must declare arg 'value' of type bool"));
	nlohmann::json dock;
	CHECK_OK(f.reg.get_dock("d", dock));
	CHECK(dock.dump() == "[]");

	std::string controls;
	for (std::size_t i = 0; i < limits::max_dock_controls; ++i)
		controls += (i ? "," : "") + std::string(R"({"type":"separator"})");
	CHECK_OK(f.reg.register_owner("d", with_dock("[" + controls + "]")));
	CHECK_FAIL(f.reg.register_owner("d", with_dock("[" + controls + R"(,{"type":"separator"}])")),
		   "too many dock controls (max 256)");
	CHECK_FAIL(f.reg.register_owner("d", with_dock("{}")), "dock must be an array");
}

void test_unregister()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("u", R"({"display_name":"U","commands":[{"id":"c"}]})"));
	bool removed = false;
	CHECK_OK(f.reg.unregister_owner("u", &removed));
	CHECK(removed);
	CHECK(f.reg.owner_count() == 0);
	CHECK_OK(f.reg.unregister_owner("u", &removed));
	CHECK(!removed);
	CHECK_OK(f.reg.unregister_owner("never.registered", &removed));
	CHECK(!removed);
	removed = true;
	CHECK_FAIL(f.reg.unregister_owner("Bad Id", &removed), "invalid owner id");
	CHECK(!removed);
	CHECK_FAIL(f.reg.set_state("u", R"({"a":1})", nullptr), "owner not registered");
	CHECK_FAIL(f.reg.check_emit("u", "evt", "{}"), "owner not registered");
	CHECK_FAIL(f.reg.check_command("u", "c", "{}"), "owner not registered");
	CHECK_FAIL(f.reg.heartbeat("u"), "owner not registered");
}

void test_set_state()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("s", R"({"display_name":"S"})"));
	std::string changes;
	CHECK_OK(f.reg.set_state("s", R"({"a":1,"b":"x"})", &changes));
	CHECK(changes == R"({"a":1,"b":"x"})");
	CHECK_OK(f.reg.set_state("s", R"({"a":1,"b":"y","c":true})", &changes));
	CHECK(changes == R"({"b":"y","c":true})");
	CHECK_OK(f.reg.set_state("s", R"({"a":null})", &changes));
	CHECK(changes == R"({"a":null})");
	CHECK_OK(f.reg.set_state("s", R"({"a":null})", &changes));
	CHECK(changes == "{}");

	CHECK_FAIL(f.reg.set_state("s", R"({"o":{"x":1}})", nullptr), "must be a string, number, boolean or null");
	CHECK_FAIL(f.reg.set_state("s", R"({"o":[1]})", nullptr), "must be a string, number, boolean or null");
	CHECK_FAIL(f.reg.set_state("s", "{\"" + repeat('k', 65) + "\":1}", nullptr), "invalid state key");
	CHECK_FAIL(f.reg.set_state("s", R"({"bad-key":1})", nullptr), "invalid state key");
	CHECK_FAIL(f.reg.set_state("s", "[]", nullptr), "must be an object");

	Fixture g;
	CHECK_OK(g.reg.register_owner("s", R"({"display_name":"S"})"));
	std::string keys;
	for (std::size_t i = 0; i < limits::max_state_keys; ++i)
		keys += (i ? "," : "") + std::string("\"k") + std::to_string(i) + "\":0";
	CHECK_OK(g.reg.set_state("s", "{" + keys + "}", nullptr));
	// All-or-nothing: the k0 change must not be applied when the call is rejected
	CHECK_FAIL(g.reg.set_state("s", R"({"k0":1,"extra":1})", nullptr), "too many state keys (max 256)");
	CHECK_OK(g.reg.set_state("s", R"({"k0":1})", &changes));
	CHECK(changes == R"({"k0":1})");
	// Deleting and adding in one call stays within the limit
	CHECK_OK(g.reg.set_state("s", R"({"k1":null,"extra":1})", nullptr));
}

void test_check_command()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner(
		"c",
		R"({"display_name":"C","commands":[{"id":"add","args":{"seconds":"int","name":"string","rate":"number","on":"bool"}}]})"));
	CHECK_OK(f.reg.check_command("c", "add", R"({"seconds":5,"name":"x","rate":1.5,"on":true})"));
	CHECK_OK(f.reg.check_command("c", "add", R"({"seconds":5.0})"));
	CHECK_OK(f.reg.check_command("c", "add", R"({"rate":2})"));
	CHECK_OK(f.reg.check_command("c", "add", ""));
	CHECK_OK(f.reg.check_command("c", "add", "{}"));
	CHECK_FAIL(f.reg.check_command("c", "nope", "{}"), "unknown command 'nope'");
	CHECK_FAIL(f.reg.check_command("c", "add", R"({"x":1})"), "unknown argument 'x'");
	CHECK_FAIL(f.reg.check_command("c", "add", R"({"seconds":1.5})"), "must be int");
	CHECK_FAIL(f.reg.check_command("c", "add", R"({"name":1})"), "must be string");
	CHECK_FAIL(f.reg.check_command("c", "add", R"({"on":1})"), "must be bool");
	CHECK_FAIL(f.reg.check_command("c", "add", "[]"), "must be an object");
	CHECK_FAIL(f.reg.check_command("c", "Bad", "{}"), "invalid command id");

	// The arguments to forward: null means omitted and is removed, on every channel
	std::string args;
	CHECK_OK(f.reg.check_command("c", "add", R"({"seconds":5,"name":null})", &args));
	CHECK(args == R"({"seconds":5})");
	CHECK_OK(f.reg.check_command("c", "add", R"({"seconds":null})", &args));
	CHECK(args == "{}");
	CHECK_OK(f.reg.check_command("c", "add", R"({"undeclared":null})", &args));
	CHECK(args == "{}");
	CHECK_OK(f.reg.check_command("c", "add", "", &args));
	CHECK(args == "{}");
	CHECK_OK(f.reg.check_command("c", "add", "{\"rate\":0.1,\"name\":\"caf\xc3\xa9\",\"on\":false,\"seconds\":-3}",
				     &args));
	CHECK(args == "{\"name\":\"caf\xc3\xa9\",\"on\":false,\"rate\":0.1,\"seconds\":-3}");
	args = "unchanged";
	CHECK_FAIL(f.reg.check_command("c", "add", R"({"seconds":"x"})", &args), "must be int");
	CHECK(args == "unchanged");
}

// Records what the procedures would send to obs-websocket
struct FakeEventSink final : EventSink {
	std::vector<std::pair<std::string, std::string>> state_calls; // owner, changes_json
	std::vector<std::string> custom_calls;

	void state_changed(const std::string &owner, const std::string &changes_json) override
	{
		state_calls.emplace_back(owner, changes_json);
	}
	void custom_event(const std::string &owner, const std::string &event, const std::string &json) override
	{
		custom_calls.push_back(owner + "/" + event + "/" + json);
	}
};

void test_state_events()
{
	Fixture f;
	FakeEventSink sink;
	CHECK_OK(f.reg.register_owner("s", R"({"display_name":"S"})"));

	// Returns the StateChanged payload for the sink's last call, or "" if none was made
	auto last_event = [&]() -> std::string {
		if (sink.state_calls.empty())
			return "";
		const auto &[owner, changes] = sink.state_calls.back();
		return event_data::state_changed(owner, changes).dump();
	};

	// Changes only
	CHECK_OK(set_state_and_notify(f.reg, "s", R"({"a":1,"b":"x"})", sink));
	CHECK(sink.state_calls.size() == 1);
	CHECK(last_event() == R"({"changes":{"a":1,"b":"x"},"owner":"s"})");

	// Removed only
	CHECK_OK(set_state_and_notify(f.reg, "s", R"({"a":null})", sink));
	CHECK(sink.state_calls.size() == 2);
	CHECK(last_event() == R"({"changes":{},"owner":"s","removed":{"a":true}})");

	// Both
	CHECK_OK(set_state_and_notify(f.reg, "s", R"({"b":"y","c":true,"a":null})", sink));
	CHECK(sink.state_calls.size() == 3);
	// "a" was already gone, so only b and c are reported, and there is no "removed"
	CHECK(last_event() == R"({"changes":{"b":"y","c":true},"owner":"s"})");
	CHECK_OK(set_state_and_notify(f.reg, "s", R"({"b":"z","c":null})", sink));
	CHECK(sink.state_calls.size() == 4);
	CHECK(last_event() == R"({"changes":{"b":"z"},"owner":"s","removed":{"c":true}})");

	// Empty result: nothing changed, so no event at all
	CHECK_OK(set_state_and_notify(f.reg, "s", R"({"b":"z"})", sink));
	CHECK_OK(set_state_and_notify(f.reg, "s", R"({"gone":null})", sink));
	CHECK(sink.state_calls.size() == 4);

	// A failed set_state sends nothing
	CHECK_FAIL(set_state_and_notify(f.reg, "s", "{broken", sink), "invalid JSON");
	CHECK_FAIL(set_state_and_notify(f.reg, "nobody", R"({"a":1})", sink), "owner not registered");
	CHECK(sink.state_calls.size() == 4);
	CHECK(sink.custom_calls.empty());
}

// Records registration notifications for the dock tests
struct RecordingSink final : EventSink {
	std::vector<std::string> calls;

	void state_changed(const std::string &owner, const std::string &changes) override
	{
		calls.push_back("state:" + owner + ":" + changes);
	}
	void custom_event(const std::string &owner, const std::string &event, const std::string &) override
	{
		calls.push_back("event:" + owner + ":" + event);
	}
	void owner_registered(const std::string &owner) override { calls.push_back("registered:" + owner); }
	void owner_unregistered(const std::string &owner) override { calls.push_back("unregistered:" + owner); }
};

void test_printable()
{
	CHECK(printable("stopwatch.2") == "stopwatch.2");
	CHECK(printable("\xe2\x80\xaeRTL\xe2\x80\xac") == "???RTL???"); // bidi override
	CHECK(printable("a\nb\tc\x01") == "a?b?c?");
	CHECK(printable(std::string("x\0y", 3)) == "x?y");
	CHECK(printable(std::string(70, 'z')) == std::string(64, 'z') + "...");
	CHECK(printable("") == "");
}

void test_rate_limiter()
{
	auto now = std::make_shared<Clock::time_point>(Clock::time_point{} + 1h);
	auto clock = [now] {
		return *now;
	};
	RateLimiter::Config config; // 30/s burst 60 per owner, 200/s burst 400 global
	RateLimiter limiter(config, clock);

	// Burst: 60 at once, then limited
	int allowed = 0;
	RateLimiter::Decision last;
	for (int i = 0; i < 100; ++i) {
		last = limiter.acquire("a");
		allowed += last.allowed ? 1 : 0;
	}
	CHECK(allowed == 60);
	CHECK(!last.allowed);
	CHECK(last.retry_after_ms == 34); // 1 token at 30/s = 33.3 ms, rounded up

	// Refill: 30/s sustained
	*now += 1s;
	allowed = 0;
	for (int i = 0; i < 100; ++i)
		allowed += limiter.acquire("a").allowed ? 1 : 0;
	CHECK(allowed == 30);
	*now += 100ms;
	CHECK(limiter.acquire("a").allowed); // 3 tokens after 100 ms
	*now += 10s;                         // refills to the burst, not beyond
	allowed = 0;
	for (int i = 0; i < 100; ++i)
		allowed += limiter.acquire("a").allowed ? 1 : 0;
	CHECK(allowed == 60);

	// Owners are independent
	CHECK(limiter.acquire("b").allowed);

	// Global cap: 400 burst across owners
	RateLimiter global(config, clock);
	allowed = 0;
	for (int owner = 0; owner < 10; ++owner) {
		for (int i = 0; i < 60; ++i)
			allowed += global.acquire("o" + std::to_string(owner)).allowed ? 1 : 0;
	}
	CHECK(allowed == 400);
	RateLimiter::Decision d = global.acquire("fresh");
	CHECK(!d.allowed);
	CHECK(d.retry_after_ms == 5); // 1 token at 200/s = 5 ms
	*now += 5ms;
	CHECK(global.acquire("fresh").allowed);

	// Many distinct owner names don't grow the map without bound
	RateLimiter many(config, clock);
	for (int i = 0; i < 2000; ++i)
		many.acquire("owner" + std::to_string(i));
	*now += 10s;
	CHECK(many.acquire("after").allowed);
}

void test_log_limiter()
{
	auto now = std::make_shared<Clock::time_point>(Clock::time_point{} + 1h);
	LogLimiter limiter(10s, [now] { return *now; });

	LogLimiter::Verdict v = limiter.check("x");
	CHECK(v.log && v.suppressed == 0);
	for (int i = 0; i < 5; ++i)
		CHECK(!limiter.check("x").log);
	CHECK(limiter.check("y").log); // other keys are independent
	*now += 9s;
	CHECK(!limiter.check("x").log);
	*now += 1s; // window over: logged again, with the count
	v = limiter.check("x");
	CHECK(v.log && v.suppressed == 6);
	CHECK(LogLimiter::suffix(v) == " (6 similar lines suppressed in the last 10 s)");
	CHECK(LogLimiter::suffix(LogLimiter::Verdict{true, 1}) == " (1 similar line suppressed in the last 10 s)");
	CHECK(LogLimiter::suffix(LogLimiter::Verdict{}).empty());
	*now += 30s; // quiet: next one is a plain first line
	v = limiter.check("x");
	CHECK(v.log && v.suppressed == 0);

	// Many distinct keys don't grow without bound, and a key with suppressed
	// lines keeps its count
	for (int i = 0; i < 1500; ++i)
		limiter.check("k" + std::to_string(i));
	CHECK(!limiter.check("x").log);
	*now += 11s;
	for (int i = 0; i < 1500; ++i)
		limiter.check("m" + std::to_string(i));
	v = limiter.check("x");
	CHECK(v.log && v.suppressed == 1);
}

void test_label_bind_and_replace_warning()
{
	Fixture f;
	// label_bind: a valid state key is kept, an invalid one skips the button
	Result r =
		f.reg.register_owner("lb", with_dock(R"([{"type":"button","command":"go","label_bind":"go_label"}])"));
	CHECK(r.ok);
	CHECK(r.warnings.empty());
	nlohmann::json dock;
	CHECK_OK(f.reg.get_dock("lb", dock));
	CHECK(dock.dump() == R"([{"command":"go","label_bind":"go_label","type":"button"}])");
	CHECK_OK(f.reg.unregister_owner("lb"));

	r = f.reg.register_owner("lb", with_dock(R"([{"type":"button","command":"go","label_bind":"bad key"}])"));
	CHECK(r.ok);
	CHECK(has_warning(r, "dock[0]: invalid or missing label_bind; skipped"));
	CHECK_OK(f.reg.get_dock("lb", dock));
	CHECK(dock.dump() == "[]");
	CHECK_OK(f.reg.unregister_owner("lb"));

	// Replacing an owner that is still active warns (two scripts sharing an owner)
	r = f.reg.register_owner("w", R"({"display_name":"W"})");
	CHECK(r.ok && r.warnings.empty()); // fresh
	r = f.reg.register_owner("w", R"({"display_name":"W2"})");
	CHECK(r.ok);
	CHECK(has_warning(r, "owner 'w' was already registered and active; its registration was replaced"));

	// ...but not after unregistering (script reload) or once the owner is stale
	CHECK_OK(f.reg.unregister_owner("w"));
	r = f.reg.register_owner("w", R"({"display_name":"W"})");
	CHECK(r.ok && r.warnings.empty());
	CHECK_OK(f.reg.heartbeat("w"));
	f.advance(31s);
	r = f.reg.register_owner("w", R"({"display_name":"W"})");
	CHECK(r.ok && r.warnings.empty());
}

void test_registration_events()
{
	Fixture f;
	RecordingSink a, b;
	FanOutEventSink both{&a, &b};

	// Notifications only on success, forwarded to every sink in order
	CHECK_OK(register_and_notify(f.reg, "x", R"({"display_name":"X"})", both));
	CHECK_FAIL(register_and_notify(f.reg, "y", "{broken", both), "invalid JSON");
	CHECK_OK(set_state_and_notify(f.reg, "x", R"({"k":1})", both));
	both.custom_event("x", "e", "{}");
	bool removed = false;
	CHECK_OK(unregister_and_notify(f.reg, "x", both, &removed));
	CHECK(removed);
	CHECK_OK(unregister_and_notify(f.reg, "x", both, &removed)); // already gone: no notification
	CHECK(!removed);
	CHECK_OK(unregister_and_notify(f.reg, "never", both));
	const std::vector<std::string> expected = {"registered:x", R"(state:x:{"k":1})", "event:x:e", "unregistered:x"};
	CHECK(a.calls == expected);
	CHECK(b.calls == expected);

	// generation changes on every (re-)registration
	CHECK_OK(f.reg.register_owner("g", R"({"display_name":"G"})"));
	auto first = f.reg.list_owners();
	CHECK_OK(f.reg.register_owner("g", R"({"display_name":"G2"})"));
	auto second = f.reg.list_owners();
	CHECK(first.size() == 1 && second.size() == 1 && second[0].generation > first[0].generation);
	CHECK(second.size() == 1 && second[0].display_name == "G2");

	// get_dock returns the validated controls only
	CHECK_OK(f.reg.register_owner("d", with_dock(R"([{"type":"separator"},{"type":"slider"}])")));
	nlohmann::json dock;
	CHECK_OK(f.reg.get_dock("d", dock));
	CHECK(dock.dump() == R"([{"type":"separator"}])");
	CHECK_FAIL(f.reg.get_dock("nobody", dock), "owner not registered");

	// The dock's Remove button on a stale owner: removed, notified, and it can come back
	RecordingSink sink;
	CHECK_OK(f.reg.register_owner("s", R"({"display_name":"S"})"));
	CHECK_OK(f.reg.heartbeat("s"));
	f.advance(31s);
	CHECK(f.reg.is_stale("s"));
	CHECK_OK(unregister_and_notify(f.reg, "s", sink));
	CHECK(sink.calls == std::vector<std::string>{"unregistered:s"});
	bool listed = false;
	for (const auto &o : f.reg.list_owners())
		listed = listed || o.id == "s";
	CHECK(!listed);
	CHECK_FAIL(f.reg.set_state("s", R"({"a":1})", nullptr), "owner not registered");
	CHECK_OK(register_and_notify(f.reg, "s", R"({"display_name":"S"})", sink));
	CHECK(!f.reg.is_stale("s"));
}

void test_dock_logic()
{
	using namespace dock_logic;
	using nlohmann::json;
	auto summary = [](std::string id, std::uint64_t gen, bool stale = false) {
		return OwnerSummary{std::move(id), "D", stale, gen};
	};

	// First registration: create, in registration order
	SectionPlan p = plan_sections({}, {summary("b", 2), summary("a", 5), summary("c", 1)});
	CHECK(p.create.size() == 3 && p.create[0].id == "c" && p.create[1].id == "b" && p.create[2].id == "a");
	CHECK(p.remove.empty() && p.rebuild.empty() && p.stale_changes.empty());

	// Nothing changed
	std::vector<ShownSection> shown = {{"c", 1, false}, {"b", 2, false}, {"a", 5, false}};
	p = plan_sections(shown, {summary("a", 5), summary("b", 2), summary("c", 1)});
	CHECK(p.create.empty() && p.remove.empty() && p.rebuild.empty() && p.stale_changes.empty());

	// Unregister: remove
	p = plan_sections(shown, {summary("a", 5), summary("c", 1)});
	CHECK(p.remove == std::vector<std::string>{"b"} && p.create.empty() && p.rebuild.empty());

	// Reload (unregister + register before the reconcile runs): one rebuild in place
	p = plan_sections(shown, {summary("a", 5), summary("b", 9), summary("c", 1)});
	CHECK(p.rebuild.size() == 1 && p.rebuild[0].id == "b" && p.create.empty() && p.remove.empty());

	// Stale flips without re-registration
	p = plan_sections(shown, {summary("a", 5, true), summary("b", 2), summary("c", 1)});
	CHECK(p.stale_changes.size() == 1 && p.stale_changes[0].first == "a" && p.stale_changes[0].second);
	p = plan_sections({{"a", 5, true}}, {summary("a", 5, false)});
	CHECK(p.stale_changes.size() == 1 && !p.stale_changes[0].second);
	// Re-registering a stale owner rebuilds it (fresh) rather than only flipping stale
	p = plan_sections({{"a", 5, true}}, {summary("a", 6, false)});
	CHECK(p.rebuild.size() == 1 && p.stale_changes.empty());

	// Removed from the dock, then registered again: a new section
	p = plan_sections({}, {summary("a", 7)});
	CHECK(p.create.size() == 1 && p.create[0].id == "a");

	// Label text
	json s = "hello", i = 42, f1 = 0.1, f2 = 2.50, t = true, n = nullptr;
	CHECK(format_state_value(&s) == "hello");
	CHECK(format_state_value(&i) == "42");
	CHECK(format_state_value(&f1) == "0.1");
	CHECK(format_state_value(&f2) == "2.5");
	CHECK(format_state_value(&t) == "true");
	CHECK(format_state_value(&n) == std::string("\xe2\x80\x94"));
	CHECK(format_state_value(nullptr) == std::string("\xe2\x80\x94"));

	// Button text with label_bind
	json starting = "Starting\xe2\x80\xa6", empty = "", number = 3;
	CHECK(button_text("Start", nullptr) == "Start");
	CHECK(button_text("Start", &n) == "Start");
	CHECK(button_text("Start", &empty) == "Start");
	CHECK(button_text("Start", &starting) == "Starting\xe2\x80\xa6");
	CHECK(button_text("Start", &number) == "3");

	// Spin box type
	CHECK(number_is_integer(json::parse(R"({"type":"number","id":"n","min":1,"max":3600,"default":1})")));
	CHECK(number_is_integer(json::parse(R"({"type":"number","id":"n"})")));
	CHECK(number_is_integer(json::parse(R"({"type":"number","id":"n","max":10.0})")));
	CHECK(!number_is_integer(json::parse(R"({"type":"number","id":"n","min":0,"max":1,"default":0.5})")));

	// Button args
	json command = json::parse(R"({"id":"add","args":{"seconds":"int","rate":"number","name":"string"}})");
	json button = json::parse(
		R"({"type":"button","command":"add","args_from":{"seconds":"secs","rate":"r","name":"nm","missing":"nope"}})");
	std::map<std::string, json> values = {{"secs", 5.0}, {"r", 1.5}, {"nm", "x"}};
	CHECK(build_button_args(button, command, values).dump() == R"({"name":"x","rate":1.5,"seconds":5})");
	values["secs"] = 1.5; // not whole: passed on so check_command reports it
	CHECK(build_button_args(button, command, values).dump() == R"({"name":"x","rate":1.5,"seconds":1.5})");
	values["nm"] = 7; // a number control feeding a string arg
	values.erase("r");
	CHECK(build_button_args(button, command, values).dump() == R"({"name":"7","seconds":1.5})");
	CHECK(build_button_args(json::parse(R"({"type":"button","command":"add"})"), command, values).dump() == "{}");
}

void test_event_data()
{
	using event_data::custom_event;
	using event_data::state_changed;

	CHECK(state_changed("o", R"({"a":1,"b":"x"})").dump() == R"({"changes":{"a":1,"b":"x"},"owner":"o"})");
	CHECK(state_changed("o", R"({"a":null})").dump() == R"({"changes":{},"owner":"o","removed":{"a":true}})");
	CHECK(state_changed("o", R"({"a":2,"b":null,"c":null})").dump() ==
	      R"({"changes":{"a":2},"owner":"o","removed":{"b":true,"c":true}})");
	// A registry change set round-trips into the event
	Fixture f;
	CHECK_OK(f.reg.register_owner("s", R"({"display_name":"S"})"));
	std::string changes;
	CHECK_OK(f.reg.set_state("s", R"({"k":1,"t":"x"})", &changes));
	CHECK_OK(f.reg.set_state("s", R"({"k":null,"t":"y"})", &changes));
	CHECK(state_changed("s", changes).dump() == R"({"changes":{"t":"y"},"owner":"s","removed":{"k":true}})");

	bool threw = false;
	try {
		state_changed("o", "{broken");
	} catch (const nlohmann::json::exception &) {
		threw = true;
	}
	CHECK(threw);

	CHECK(custom_event("o", "e.x", R"({"items":[{"id":1}],"s":"v"})").dump() ==
	      R"({"data":{"items":[{"id":1}],"s":"v"},"event":"e.x","owner":"o"})");
	CHECK(custom_event("o", "e", "{}").dump() == R"({"data":{},"event":"e","owner":"o"})");
}

void test_check_emit()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("e", R"({"display_name":"E"})"));
	CHECK_OK(f.reg.check_emit("e", "score.changed", R"({"home":1})"));
	CHECK_OK(f.reg.check_emit("e", "ping", ""));
	CHECK_FAIL(f.reg.check_emit("e", "bad event", "{}"), "invalid event name");
	CHECK_FAIL(f.reg.check_emit("e", "", "{}"), "invalid event name");
	CHECK_FAIL(f.reg.check_emit("e", "big", "{\"p\":\"" + repeat('a', limits::max_json_bytes) + "\"}"),
		   "json exceeds");
	CHECK_FAIL(f.reg.check_emit("e", "x", "5"), "must be an object");

	// obs_data can't carry null or arrays of non-objects, so they are rejected
	const char *obs_data_error = "json cannot contain null or arrays of non-objects";
	CHECK_FAIL(f.reg.check_emit("e", "x", R"({"a":null})"), obs_data_error);
	CHECK_FAIL(f.reg.check_emit("e", "x", R"({"a":{"b":{"c":null}}})"), obs_data_error);
	CHECK_FAIL(f.reg.check_emit("e", "x", R"({"tags":["a","b"]})"), obs_data_error);
	CHECK_FAIL(f.reg.check_emit("e", "x", R"({"n":[1,2]})"), obs_data_error);
	CHECK_FAIL(f.reg.check_emit("e", "x", R"({"m":[{"ok":1},[{"x":1}]]})"), obs_data_error);
	CHECK_FAIL(f.reg.check_emit("e", "x", R"({"m":[{"inner":[true]}]})"), obs_data_error);
	CHECK_OK(f.reg.check_emit("e", "x", R"({"items":[{"id":1},{"id":2,"sub":{"s":"x"}}],"empty":[],"o":{}})"));
	std::string deep = R"({"d":)" + repeat('[', 10000) + repeat(']', 10000) + "}";
	CHECK_FAIL(f.reg.check_emit("e", "x", deep), obs_data_error);
}

void test_heartbeat_and_stale()
{
	Fixture f;
	CHECK_OK(f.reg.register_owner("h", R"({"display_name":"H","commands":[{"id":"c"}]})"));
	f.advance(1h);
	CHECK(!f.reg.is_stale("h")); // never sent a heartbeat

	CHECK_OK(f.reg.heartbeat("h"));
	f.advance(29s);
	CHECK(!f.reg.is_stale("h"));
	CHECK_OK(f.reg.check_command("h", "c", ""));
	f.advance(2s);
	CHECK(f.reg.is_stale("h"));
	CHECK_FAIL(f.reg.check_command("h", "c", ""), "owner is stale");
	CHECK_FAIL(f.reg.heartbeat("h"), "owner is stale; register again");
	CHECK_OK(f.reg.set_state("h", R"({"a":1})", nullptr));
	CHECK_OK(f.reg.check_emit("h", "evt", ""));

	CHECK_OK(f.reg.register_owner("h", R"({"display_name":"H","commands":[{"id":"c"}]})"));
	CHECK(!f.reg.is_stale("h"));
	CHECK_OK(f.reg.check_command("h", "c", ""));
	CHECK(!f.reg.is_stale("unknown"));
}

void test_robustness()
{
	Fixture f;
	CHECK_FAIL(f.reg.register_owner("", "{}"), "missing owner");
	CHECK_FAIL(f.reg.register_owner("a", ""), "missing json");
	CHECK_FAIL(f.reg.set_state("", "{}", nullptr), "missing owner");
	CHECK_FAIL(f.reg.unregister_owner(""), "missing owner");
	CHECK_FAIL(f.reg.heartbeat(""), "missing owner");
	CHECK_FAIL(f.reg.check_emit("", "e", ""), "missing owner");
	CHECK_FAIL(f.reg.check_command("", "c", ""), "missing owner");

	std::string deep = R"({"display_name":"x","deep":)" + repeat('[', 10000) + repeat(']', 10000) + "}";
	CHECK(deep.size() <= limits::max_json_bytes);
	CHECK_OK(f.reg.register_owner("deep", deep));
	std::string deep_state = R"({"v":)" + repeat('[', 10000) + repeat(']', 10000) + "}";
	CHECK_FAIL(f.reg.set_state("deep", deep_state, nullptr), "must be a string, number, boolean or null");

	std::string garbage;
	for (int i = 0; i < 4096; ++i)
		garbage += static_cast<char>((i * 131 + 7) % 256);
	CHECK_FAIL(f.reg.register_owner("g", garbage), "invalid JSON");
	CHECK_FAIL(f.reg.set_state("deep", garbage, nullptr), "invalid JSON");
	CHECK_FAIL(f.reg.check_command("deep", "c", garbage), "invalid JSON");
	CHECK_FAIL(f.reg.register_owner(garbage, "{}"), "invalid owner id");
}

void test_snapshots()
{
	Fixture f;
	CHECK(f.reg.list_owners().empty());
	CHECK_OK(f.reg.register_owner("zeta", R"({"display_name":"Zeta"})"));
	CHECK_OK(f.reg.register_owner(
		"alpha",
		R"({"display_name":"Alpha","commands":[{"id":"start","label":"Go","description":"d","confirm":true},)"
		R"({"id":"add","args":{"seconds":"int","name":"string"}}]})"));

	auto owners = f.reg.list_owners();
	CHECK(owners.size() == 2);
	CHECK(owners.size() == 2 && owners[0].id == "alpha" && owners[0].display_name == "Alpha" && !owners[0].stale);
	CHECK(owners.size() == 2 && owners[1].id == "zeta" && owners[1].display_name == "Zeta");

	CHECK_OK(f.reg.heartbeat("zeta"));
	f.advance(31s);
	owners = f.reg.list_owners();
	CHECK(owners.size() == 2 && !owners[0].stale && owners[1].stale);

	nlohmann::json commands;
	CHECK_OK(f.reg.get_commands("alpha", commands));
	CHECK(commands.dump() ==
	      R"([{"args":{},"confirm":true,"description":"d","id":"start","label":"Go"},)"
	      R"({"args":{"name":"string","seconds":"int"},"confirm":false,"description":"","id":"add","label":"add"}])");
	CHECK_OK(f.reg.get_commands("zeta", commands));
	CHECK(commands.dump() == "[]");
	CHECK_FAIL(f.reg.get_commands("nobody", commands), "owner not registered");
	CHECK_FAIL(f.reg.get_commands("Bad", commands), "invalid owner id");

	nlohmann::json state;
	CHECK_OK(f.reg.get_state("alpha", state));
	CHECK(state.dump() == "{}");
	CHECK_OK(f.reg.set_state("alpha", R"({"a":1,"b":"x","c":true})", nullptr));
	CHECK_OK(f.reg.set_state("alpha", R"({"b":null})", nullptr));
	CHECK_OK(f.reg.get_state("alpha", state));
	CHECK(state.dump() == R"({"a":1,"c":true})");
	// The snapshot is independent of later changes
	CHECK_OK(f.reg.set_state("alpha", R"({"a":2})", nullptr));
	CHECK(state.dump() == R"({"a":1,"c":true})");
	CHECK_FAIL(f.reg.get_state("nobody", state), "owner not registered");

	f.reg.clear();
	CHECK(f.reg.list_owners().empty());
}

void test_threads()
{
	Fixture f;
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; ++t) {
		threads.emplace_back([&f, t] {
			for (int i = 0; i < 1000; ++i) {
				std::string owner = "t" + std::to_string(t) + "." + std::to_string(i % 4);
				f.reg.register_owner(owner, R"({"display_name":"T","commands":[{"id":"c"}]})");
				f.reg.set_state(owner, R"({"i":)" + std::to_string(i) + "}", nullptr);
				f.reg.check_command(owner, "c", "");
				f.reg.check_emit(owner, "e", "");
				f.reg.heartbeat(owner);
				nlohmann::json snapshot;
				f.reg.get_commands(owner, snapshot);
				f.reg.get_state(owner, snapshot);
				f.reg.list_owners();
				if (i % 3 == 0)
					f.reg.unregister_owner(owner);
			}
			for (int k = 0; k < 4; ++k)
				f.reg.unregister_owner("t" + std::to_string(t) + "." + std::to_string(k));
		});
	}
	for (auto &th : threads)
		th.join();
	CHECK(f.reg.owner_count() == 0);
}

} // namespace

int main()
{
	test_id_validation();
	test_register();
	test_json_size_and_parsing();
	test_command_limits();
	test_owner_limit();
	test_reregister();
	test_dock();
	test_unregister();
	test_set_state();
	test_check_command();
	test_check_emit();
	test_heartbeat_and_stale();
	test_robustness();
	test_event_data();
	test_registration_events();
	test_label_bind_and_replace_warning();
	test_printable();
	test_rate_limiter();
	test_log_limiter();
	test_dock_logic();
	test_state_events();
	test_snapshots();
	test_threads();

	std::printf("registry-tests: %d checks, %d failed\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
