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

#include "dock-logic.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace luabridge::dock_logic {

using json = nlohmann::json;

const char *const missing_value = "\xe2\x80\x94";

SectionPlan plan_sections(const std::vector<ShownSection> &shown, const std::vector<OwnerSummary> &registered)
{
	SectionPlan plan;
	std::map<std::string, const OwnerSummary *> by_id;
	for (const auto &o : registered)
		by_id[o.id] = &o;

	std::set<std::string> shown_ids;
	for (const auto &s : shown) {
		shown_ids.insert(s.owner);
		auto it = by_id.find(s.owner);
		if (it == by_id.end()) {
			plan.remove.push_back(s.owner);
		} else if (it->second->generation != s.generation) {
			plan.rebuild.push_back(*it->second);
		} else if (it->second->stale != s.stale) {
			plan.stale_changes.emplace_back(s.owner, it->second->stale);
		}
	}

	for (const auto &o : registered) {
		if (shown_ids.count(o.id) == 0)
			plan.create.push_back(o);
	}
	std::sort(plan.create.begin(), plan.create.end(),
		  [](const OwnerSummary &a, const OwnerSummary &b) { return a.generation < b.generation; });
	return plan;
}

std::string format_state_value(const json *value)
{
	if (!value || value->is_null())
		return missing_value;
	if (value->is_string())
		return value->get<std::string>();
	if (value->is_boolean())
		return value->get<bool>() ? "true" : "false";
	// Numbers: nlohmann prints the shortest form that round-trips (2.5, 0.1, 42)
	return value->dump();
}

namespace {

bool is_whole(const json &v)
{
	if (v.is_number_integer())
		return true;
	if (!v.is_number_float())
		return false;
	double d = v.get<double>();
	return std::isfinite(d) && std::floor(d) == d;
}

} // namespace

bool number_is_integer(const json &control)
{
	for (const char *key : {"min", "max", "default"}) {
		auto it = control.find(key);
		if (it != control.end() && !is_whole(*it))
			return false;
	}
	return true;
}

json build_button_args(const json &button, const json &command, const std::map<std::string, json> &input_values)
{
	json args = json::object();
	auto args_from = button.find("args_from");
	if (args_from == button.end() || !args_from->is_object())
		return args;
	auto declared = command.find("args");

	for (auto it = args_from->begin(); it != args_from->end(); ++it) {
		if (!it->is_string())
			continue;
		auto value = input_values.find(it->get<std::string>());
		if (value == input_values.end() || value->second.is_null())
			continue;

		std::string type;
		if (declared != command.end() && declared->is_object()) {
			auto t = declared->find(it.key());
			if (t != declared->end() && t->is_string())
				type = t->get<std::string>();
		}

		const json &v = value->second;
		if (type == "int" && v.is_number_float() && is_whole(v))
			args[it.key()] = static_cast<long long>(v.get<double>());
		else if (type == "string" && v.is_number())
			args[it.key()] = v.dump();
		else
			args[it.key()] = v;
	}
	return args;
}

} // namespace luabridge::dock_logic
