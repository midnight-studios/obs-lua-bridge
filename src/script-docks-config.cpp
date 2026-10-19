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

#include "script-docks-config.hpp"

#include <algorithm>
#include <map>

#include <nlohmann/json.hpp>

namespace luabridge::script_docks {

using json = nlohmann::json;

namespace {

// A string field of an entry: missing gives "", any other type a warning
std::string string_field(const json &entry, const char *key, const std::string &owner,
			 std::vector<std::string> &warnings)
{
	auto it = entry.find(key);
	if (it == entry.end() || it->is_null())
		return {};
	if (!it->is_string()) {
		warnings.push_back("dock '" + printable(owner) + "': " + key + " is not a string; ignored");
		return {};
	}
	return clean_title(it->get_ref<const std::string &>());
}

} // namespace

ParseResult parse_config(std::string_view text)
{
	ParseResult result;
	json doc = json::parse(text.begin(), text.end(), nullptr, false);
	if (doc.is_discarded() || !doc.is_object()) {
		result.ok = false;
		result.error = "not a JSON object";
		return result;
	}
	auto version = doc.find("version");
	if (version == doc.end() || !version->is_number()) {
		result.ok = false;
		result.error = "missing version";
		return result;
	}
	if (!version->is_number_integer() || version->get<long long>() != config_version) {
		result.ok = false;
		result.error = "unsupported version " + version->dump();
		return result;
	}
	auto docks = doc.find("docks");
	if (docks == doc.end())
		return result;
	if (!docks->is_array()) {
		result.ok = false;
		result.error = "docks is not a list";
		return result;
	}

	for (const auto &entry : *docks) {
		auto owner_it = entry.is_object() ? entry.find("owner") : entry.end();
		if (!entry.is_object() || owner_it == entry.end() || !owner_it->is_string() ||
		    !is_valid_owner_id(owner_it->get_ref<const std::string &>())) {
			result.warnings.push_back("skipped an entry without a valid owner id");
			continue;
		}
		const std::string &owner = owner_it->get_ref<const std::string &>();
		if (find_dock(result.config, owner)) {
			result.warnings.push_back("dock '" + owner + "' is listed twice; kept the first");
			continue;
		}
		if (result.config.docks.size() >= max_docks) {
			result.warnings.push_back("more than " + std::to_string(max_docks) + " docks; skipped '" +
						  owner + "'");
			continue;
		}
		ScriptDock dock;
		dock.owner = owner;
		dock.title = string_field(entry, "title", owner, result.warnings);
		dock.display_name = string_field(entry, "display_name", owner, result.warnings);
		result.config.docks.push_back(std::move(dock));
	}
	return result;
}

std::string dump_config(const Config &config)
{
	json docks = json::array();
	for (const auto &dock : config.docks) {
		json entry = {{"owner", dock.owner}};
		if (!dock.title.empty())
			entry["title"] = dock.title;
		if (!dock.display_name.empty())
			entry["display_name"] = dock.display_name;
		docks.push_back(std::move(entry));
	}
	// Replace invalid UTF-8 rather than throw; titles are cleaned, but be safe
	return json{{"version", config_version}, {"docks", std::move(docks)}}.dump(2, ' ', false,
										   json::error_handler_t::replace) +
	       "\n";
}

std::string dock_id(std::string_view owner)
{
	return "luabridge." + std::string(owner);
}

std::string dock_title(const ScriptDock &dock)
{
	if (!dock.title.empty())
		return dock.title;
	if (!dock.display_name.empty())
		return dock.display_name;
	return dock.owner;
}

std::string clean_title(std::string_view title)
{
	std::string out;
	for (char c : title) {
		auto u = static_cast<unsigned char>(c);
		if (u < 0x20 || u == 0x7f)
			continue;
		out.push_back(c);
	}
	auto is_space = [](char c) {
		return c == ' ';
	};
	out.erase(out.begin(), std::find_if_not(out.begin(), out.end(), is_space));
	out.erase(std::find_if_not(out.rbegin(), out.rend(), is_space).base(), out.end());
	if (out.size() > max_title) {
		std::size_t cut = max_title;
		// Back up over continuation bytes (10xxxxxx) to the start of a character
		while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80)
			--cut;
		out.resize(cut);
		out.erase(std::find_if_not(out.rbegin(), out.rend(), is_space).base(), out.end());
	}
	return out;
}

ScriptDock *find_dock(Config &config, std::string_view owner)
{
	for (auto &dock : config.docks) {
		if (dock.owner == owner)
			return &dock;
	}
	return nullptr;
}

const ScriptDock *find_dock(const Config &config, std::string_view owner)
{
	return find_dock(const_cast<Config &>(config), owner);
}

AddResult add_dock(Config &config, std::string_view owner, std::string_view display_name)
{
	if (!is_valid_owner_id(owner))
		return AddResult::invalid_owner;
	if (find_dock(config, owner))
		return AddResult::already_added;
	if (config.docks.size() >= max_docks)
		return AddResult::limit_reached;
	ScriptDock dock;
	dock.owner = std::string(owner);
	dock.display_name = clean_title(display_name);
	config.docks.push_back(std::move(dock));
	return AddResult::added;
}

bool remove_dock(Config &config, std::string_view owner)
{
	auto it = std::find_if(config.docks.begin(), config.docks.end(),
			       [owner](const ScriptDock &dock) { return dock.owner == owner; });
	if (it == config.docks.end())
		return false;
	config.docks.erase(it);
	return true;
}

bool note_display_name(Config &config, std::string_view owner, std::string_view display_name)
{
	ScriptDock *dock = find_dock(config, owner);
	if (!dock)
		return false;
	std::string name = clean_title(display_name);
	if (name.empty() || name == dock->display_name)
		return false;
	dock->display_name = std::move(name);
	return true;
}

std::vector<ManagerRow> manager_rows(const Config &config, const std::vector<OwnerSummary> &owners)
{
	std::map<std::string, ManagerRow> rows;
	for (const auto &owner : owners) {
		ManagerRow row;
		row.owner = owner.id;
		row.display_name = owner.display_name.empty() ? owner.id : owner.display_name;
		row.status = owner.stale ? OwnerStatus::not_responding : OwnerStatus::running;
		rows[owner.id] = std::move(row);
	}
	for (const auto &dock : config.docks) {
		auto [it, inserted] = rows.try_emplace(dock.owner);
		ManagerRow &row = it->second;
		if (inserted) {
			row.owner = dock.owner;
			row.display_name = dock.display_name.empty() ? dock.owner : dock.display_name;
			row.status = OwnerStatus::not_loaded;
		}
		row.docked = true;
		row.title = dock.title;
	}
	std::vector<ManagerRow> out;
	out.reserve(rows.size());
	for (auto &[id, row] : rows)
		out.push_back(std::move(row));
	return out;
}

} // namespace luabridge::script_docks
