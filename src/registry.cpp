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

#include "registry.hpp"

#include <cmath>
#include <set>

namespace luabridge {

using json = nlohmann::json;

namespace {

bool is_lower_or_digit(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool is_alnum(char c)
{
	return is_lower_or_digit(c) || (c >= 'A' && c <= 'Z');
}

template<typename Pred> bool matches(std::string_view s, Pred allowed)
{
	if (s.empty() || s.size() > limits::max_id_length)
		return false;
	for (char c : s) {
		if (!allowed(c))
			return false;
	}
	return true;
}

std::string quote_id(std::string_view s)
{
	// Error messages echo ids back; keep them short and printable
	std::string out = "'";
	for (char c : s.substr(0, 64)) {
		unsigned char u = static_cast<unsigned char>(c);
		out += (u >= 0x20 && u < 0x7f) ? c : '?';
	}
	if (s.size() > 64)
		out += "...";
	out += "'";
	return out;
}

std::string dump(const json &j)
{
	return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

// Parses text into a JSON object. Size is checked before parsing.
bool parse_object(std::string_view text, bool empty_is_object, json &out, std::string &error)
{
	if (text.size() > limits::max_json_bytes) {
		error = "json exceeds " + std::to_string(limits::max_json_bytes) + " bytes";
		return false;
	}
	if (text.empty()) {
		if (empty_is_object) {
			out = json::object();
			return true;
		}
		error = "missing json";
		return false;
	}
	out = json::parse(text.begin(), text.end(), nullptr, false);
	if (out.is_discarded()) {
		error = "invalid JSON";
		return false;
	}
	if (!out.is_object()) {
		error = "json must be an object";
		return false;
	}
	return true;
}

std::optional<ArgType> parse_arg_type(const json &j)
{
	if (!j.is_string())
		return std::nullopt;
	const auto &s = j.get_ref<const std::string &>();
	if (s == "int")
		return ArgType::Int;
	if (s == "number")
		return ArgType::Number;
	if (s == "string")
		return ArgType::String;
	if (s == "bool")
		return ArgType::Bool;
	return std::nullopt;
}

const char *arg_type_name(ArgType type)
{
	switch (type) {
	case ArgType::Int:
		return "int";
	case ArgType::Number:
		return "number";
	case ArgType::String:
		return "string";
	case ArgType::Bool:
		return "bool";
	}
	return "?";
}

bool arg_matches(ArgType type, const json &value)
{
	switch (type) {
	case ArgType::Int:
		if (value.is_number_integer())
			return true;
		if (value.is_number_float()) {
			double d = value.get<double>();
			return std::isfinite(d) && std::floor(d) == d;
		}
		return false;
	case ArgType::Number:
		return value.is_number();
	case ArgType::String:
		return value.is_string();
	case ArgType::Bool:
		return value.is_boolean();
	}
	return false;
}

// Optional string field with a byte limit. Returns false with error if present but invalid.
bool optional_string(const json &obj, const char *key, std::size_t max_len, std::string &out, std::string &error)
{
	auto it = obj.find(key);
	if (it == obj.end())
		return true;
	if (!it->is_string() || it->get_ref<const std::string &>().size() > max_len) {
		error = std::string(key) + " must be a string of at most " + std::to_string(max_len) + " bytes";
		return false;
	}
	out = it->get<std::string>();
	return true;
}

bool parse_command(const json &j, Command &cmd, std::string &error)
{
	if (!j.is_object()) {
		error = "must be an object";
		return false;
	}
	auto id = j.find("id");
	if (id == j.end() || !id->is_string() || !is_valid_command_id(id->get_ref<const std::string &>())) {
		error = "invalid or missing id";
		return false;
	}
	cmd.id = id->get<std::string>();
	cmd.label = cmd.id;
	if (!optional_string(j, "label", limits::max_label, cmd.label, error) ||
	    !optional_string(j, "description", limits::max_description, cmd.description, error))
		return false;

	auto confirm = j.find("confirm");
	if (confirm != j.end()) {
		if (!confirm->is_boolean()) {
			error = "confirm must be a boolean";
			return false;
		}
		cmd.confirm = confirm->get<bool>();
	}

	auto args = j.find("args");
	if (args != j.end()) {
		if (!args->is_object()) {
			error = "args must be an object";
			return false;
		}
		if (args->size() > limits::max_args) {
			error = "too many args (max " + std::to_string(limits::max_args) + ")";
			return false;
		}
		for (auto it = args->begin(); it != args->end(); ++it) {
			if (!is_valid_command_id(it.key())) {
				error = "invalid arg name " + quote_id(it.key());
				return false;
			}
			auto type = parse_arg_type(it.value());
			if (!type) {
				error = "arg " + quote_id(it.key()) + " must have type int, number, string or bool";
				return false;
			}
			cmd.args.emplace_back(it.key(), *type);
		}
	}
	return true;
}

const Command *find_command(const std::vector<Command> &commands, std::string_view id)
{
	for (const auto &c : commands) {
		if (c.id == id)
			return &c;
	}
	return nullptr;
}

bool has_arg(const Command &cmd, std::string_view name)
{
	for (const auto &a : cmd.args) {
		if (a.first == name)
			return true;
	}
	return false;
}

class DockValidator {
public:
	DockValidator(const std::vector<Command> &commands, std::vector<std::string> &warnings)
		: commands_(commands),
		  warnings_(warnings)
	{
	}

	// Returns false with error only for structural problems that reject the registration.
	bool validate(const json &dock, json &out, std::string &error)
	{
		if (!dock.is_array()) {
			error = "dock must be an array";
			return false;
		}
		if (count_controls(dock) > limits::max_dock_controls) {
			error = "too many dock controls (max " + std::to_string(limits::max_dock_controls) + ")";
			return false;
		}
		collect_input_ids(dock);

		out = json::array();
		for (std::size_t i = 0; i < dock.size(); ++i) {
			std::string where = "dock[" + std::to_string(i) + "]";
			json control;
			if (validate_control(dock[i], where, false, control))
				out.push_back(std::move(control));
		}
		return true;
	}

private:
	static std::size_t count_controls(const json &dock)
	{
		std::size_t n = 0;
		for (const auto &c : dock) {
			++n;
			if (c.is_object()) {
				auto items = c.find("items");
				if (items != c.end() && items->is_array())
					n += items->size();
			}
		}
		return n;
	}

	static std::string type_of(const json &c)
	{
		if (!c.is_object())
			return {};
		auto t = c.find("type");
		return (t != c.end() && t->is_string()) ? t->get<std::string>() : std::string();
	}

	// Ids of input controls (number, text) that buttons can read args from
	void collect_input_ids(const json &dock)
	{
		for (const auto &c : dock) {
			std::string type = type_of(c);
			if (type == "row") {
				auto items = c.find("items");
				if (items != c.end() && items->is_array())
					collect_input_ids(*items);
			} else if (type == "number" || type == "text") {
				auto id = c.find("id");
				if (id != c.end() && id->is_string() &&
				    is_valid_command_id(id->get_ref<const std::string &>()))
					input_ids_.insert(id->get<std::string>());
			}
		}
	}

	void skip(const std::string &where, const std::string &why)
	{
		warnings_.push_back(where + ": " + why + "; skipped");
	}

	bool require_state_key(const json &c, const char *key, const std::string &where)
	{
		auto it = c.find(key);
		if (it == c.end() || !it->is_string() || !is_valid_state_key(it->get_ref<const std::string &>())) {
			skip(where, std::string("invalid or missing ") + key);
			return false;
		}
		return true;
	}

	const Command *require_command(const json &c, const std::string &where)
	{
		auto it = c.find("command");
		if (it == c.end() || !it->is_string()) {
			skip(where, "missing command");
			return nullptr;
		}
		const Command *cmd = find_command(commands_, it->get_ref<const std::string &>());
		if (!cmd)
			skip(where, "command " + quote_id(it->get_ref<const std::string &>()) + " is not declared");
		return cmd;
	}

	bool require_new_id(const json &c, const std::string &where)
	{
		auto it = c.find("id");
		if (it == c.end() || !it->is_string() || !is_valid_command_id(it->get_ref<const std::string &>())) {
			skip(where, "invalid or missing id");
			return false;
		}
		if (!seen_ids_.insert(it->get<std::string>()).second) {
			skip(where, "duplicate id " + quote_id(it->get_ref<const std::string &>()));
			return false;
		}
		return true;
	}

	bool optional_number(const json &c, const char *key, const std::string &where, std::optional<double> &out)
	{
		auto it = c.find(key);
		if (it == c.end())
			return true;
		if (!it->is_number()) {
			skip(where, std::string(key) + " must be a number");
			return false;
		}
		out = it->get<double>();
		return true;
	}

	bool validate_control(const json &c, const std::string &where, bool in_row, json &out)
	{
		if (!c.is_object()) {
			skip(where, "control must be an object");
			return false;
		}
		std::string type = type_of(c);
		if (type.empty()) {
			skip(where, "missing type");
			return false;
		}

		if (type == "separator") {
			// nothing to check
		} else if (type == "label") {
			if (!require_state_key(c, "bind", where))
				return false;
			auto style = c.find("style");
			if (style != c.end() && !style->is_string()) {
				skip(where, "style must be a string");
				return false;
			}
		} else if (type == "button") {
			const Command *cmd = require_command(c, where);
			if (!cmd)
				return false;
			auto args_from = c.find("args_from");
			if (args_from != c.end()) {
				if (!args_from->is_object()) {
					skip(where, "args_from must be an object");
					return false;
				}
				for (auto it = args_from->begin(); it != args_from->end(); ++it) {
					if (!has_arg(*cmd, it.key())) {
						skip(where, "args_from names undeclared arg " + quote_id(it.key()));
						return false;
					}
					if (!it->is_string() || input_ids_.count(it->get<std::string>()) == 0) {
						skip(where, "args_from " + quote_id(it.key()) +
								    " must name a number or text control");
						return false;
					}
				}
			}
		} else if (type == "toggle") {
			if (!require_state_key(c, "bind", where) || !require_command(c, where))
				return false;
		} else if (type == "number") {
			if (!require_new_id(c, where))
				return false;
			std::optional<double> min, max, def;
			if (!optional_number(c, "min", where, min) || !optional_number(c, "max", where, max) ||
			    !optional_number(c, "default", where, def))
				return false;
			if ((min && max && *min > *max) || (def && min && *def < *min) || (def && max && *def > *max)) {
				skip(where, "requires min <= default <= max");
				return false;
			}
		} else if (type == "text") {
			if (!require_new_id(c, where))
				return false;
			std::string def, error;
			if (!optional_string(c, "default", limits::max_text, def, error)) {
				skip(where, error);
				return false;
			}
		} else if (type == "row") {
			if (in_row) {
				skip(where, "rows cannot be nested");
				return false;
			}
			auto items = c.find("items");
			if (items == c.end() || !items->is_array()) {
				skip(where, "row requires an items array");
				return false;
			}
			json kept = json::array();
			for (std::size_t i = 0; i < items->size(); ++i) {
				json item;
				if (validate_control((*items)[i], where + ".items[" + std::to_string(i) + "]", true,
						     item))
					kept.push_back(std::move(item));
			}
			out = c;
			out["items"] = std::move(kept);
			return true;
		} else {
			skip(where, "unknown control type " + quote_id(type));
			return false;
		}

		out = c;
		return true;
	}

	const std::vector<Command> &commands_;
	std::vector<std::string> &warnings_;
	std::set<std::string> input_ids_;
	std::set<std::string> seen_ids_;
};

bool build_owner(std::string_view owner_id, const json &j, Owner &owner, Result &result)
{
	owner.id = std::string(owner_id);

	auto name = j.find("display_name");
	if (name == j.end() || !name->is_string() || name->get_ref<const std::string &>().empty() ||
	    name->get_ref<const std::string &>().size() > limits::max_display_name) {
		result = Result::failure("display_name must be a string of 1-" +
					 std::to_string(limits::max_display_name) + " bytes");
		return false;
	}
	owner.display_name = name->get<std::string>();

	auto commands = j.find("commands");
	if (commands != j.end()) {
		if (!commands->is_array()) {
			result = Result::failure("commands must be an array");
			return false;
		}
		if (commands->size() > limits::max_commands) {
			result =
				Result::failure("too many commands (max " + std::to_string(limits::max_commands) + ")");
			return false;
		}
		for (std::size_t i = 0; i < commands->size(); ++i) {
			Command cmd;
			std::string error;
			if (!parse_command((*commands)[i], cmd, error)) {
				result = Result::failure("commands[" + std::to_string(i) + "]: " + error);
				return false;
			}
			if (find_command(owner.commands, cmd.id)) {
				result = Result::failure("duplicate command " + quote_id(cmd.id));
				return false;
			}
			owner.commands.push_back(std::move(cmd));
		}
	}

	auto dock = j.find("dock");
	if (dock != j.end()) {
		std::string error;
		DockValidator validator(owner.commands, result.warnings);
		if (!validator.validate(*dock, owner.dock, error)) {
			result = Result::failure(error);
			return false;
		}
	}
	return true;
}

// Runs fn and converts any unexpected exception into an error result
template<typename Fn> Result guarded(Fn &&fn)
{
	try {
		return fn();
	} catch (const std::exception &e) {
		return Result::failure(std::string("internal error: ") + e.what());
	} catch (...) {
		return Result::failure("internal error");
	}
}

Result check_owner_arg(std::string_view owner)
{
	if (owner.empty())
		return Result::failure("missing owner");
	if (!is_valid_owner_id(owner))
		return Result::failure("invalid owner id " + quote_id(owner));
	return {};
}

} // namespace

bool is_valid_owner_id(std::string_view id)
{
	return matches(id, [](char c) { return is_lower_or_digit(c) || c == '_' || c == '.' || c == '-'; });
}

bool is_valid_command_id(std::string_view id)
{
	return matches(id, [](char c) { return is_lower_or_digit(c) || c == '_'; });
}

bool is_valid_state_key(std::string_view key)
{
	return matches(key, [](char c) { return is_alnum(c) || c == '_' || c == '.'; });
}

bool is_valid_event_name(std::string_view name)
{
	return is_valid_state_key(name);
}

Registry::Registry(std::function<Clock::time_point()> now) : now_(std::move(now)) {}

bool Registry::is_stale_locked(const Owner &owner) const
{
	return owner.last_heartbeat && now_() - *owner.last_heartbeat > limits::stale_after;
}

Result Registry::register_owner(std::string_view owner_id, std::string_view text)
{
	return guarded([&] {
		Result r = check_owner_arg(owner_id);
		if (!r.ok)
			return r;

		json j;
		std::string error;
		if (!parse_object(text, false, j, error))
			return Result::failure(error);

		Owner owner;
		if (!build_owner(owner_id, j, owner, r))
			return r;

		std::lock_guard lock(mutex_);
		auto it = owners_.find(owner_id);
		if (it == owners_.end()) {
			if (owners_.size() >= limits::max_owners)
				return Result::failure("too many owners (max " + std::to_string(limits::max_owners) +
						       ")");
			std::string key = owner.id;
			owners_.emplace(std::move(key), std::move(owner));
		} else {
			it->second = std::move(owner);
		}
		return r;
	});
}

Result Registry::unregister_owner(std::string_view owner_id, bool *removed)
{
	if (removed)
		*removed = false;
	return guarded([&] {
		Result r = check_owner_arg(owner_id);
		if (!r.ok)
			return r;

		std::lock_guard lock(mutex_);
		auto it = owners_.find(owner_id);
		if (it != owners_.end()) {
			owners_.erase(it);
			if (removed)
				*removed = true;
		}
		return r;
	});
}

Result Registry::set_state(std::string_view owner_id, std::string_view text, std::string *changes_json)
{
	return guarded([&] {
		Result r = check_owner_arg(owner_id);
		if (!r.ok)
			return r;

		json j;
		std::string error;
		if (!parse_object(text, false, j, error))
			return Result::failure(error);
		for (auto it = j.begin(); it != j.end(); ++it) {
			if (!is_valid_state_key(it.key()))
				return Result::failure("invalid state key " + quote_id(it.key()));
			const json &v = it.value();
			if (!(v.is_string() || v.is_number() || v.is_boolean() || v.is_null()))
				return Result::failure("state value for " + quote_id(it.key()) +
						       " must be a string, number, boolean or null");
		}

		json changes = json::object();
		{
			std::lock_guard lock(mutex_);
			auto owner = owners_.find(owner_id);
			if (owner == owners_.end())
				return Result::failure("owner not registered");

			// Apply to a copy so a rejected update leaves the state untouched
			auto state = owner->second.state;
			for (auto it = j.begin(); it != j.end(); ++it) {
				auto existing = state.find(it.key());
				if (it->is_null()) {
					if (existing != state.end()) {
						state.erase(existing);
						changes[it.key()] = nullptr;
					}
				} else if (existing == state.end() || existing->second != it.value()) {
					state[it.key()] = it.value();
					changes[it.key()] = it.value();
				}
			}
			if (state.size() > limits::max_state_keys)
				return Result::failure("too many state keys (max " +
						       std::to_string(limits::max_state_keys) + ")");
			owner->second.state = std::move(state);
		}

		if (changes_json)
			*changes_json = dump(changes);
		return r;
	});
}

Result Registry::check_emit(std::string_view owner_id, std::string_view event, std::string_view text)
{
	return guarded([&] {
		Result r = check_owner_arg(owner_id);
		if (!r.ok)
			return r;
		if (!is_valid_event_name(event))
			return Result::failure("invalid event name " + quote_id(event));

		json j;
		std::string error;
		if (!parse_object(text, true, j, error))
			return Result::failure(error);

		std::lock_guard lock(mutex_);
		if (owners_.find(owner_id) == owners_.end())
			return Result::failure("owner not registered");
		return r;
	});
}

Result Registry::check_command(std::string_view owner_id, std::string_view command, std::string_view text)
{
	return guarded([&] {
		Result r = check_owner_arg(owner_id);
		if (!r.ok)
			return r;
		if (!is_valid_command_id(command))
			return Result::failure("invalid command id " + quote_id(command));

		json j;
		std::string error;
		if (!parse_object(text, true, j, error))
			return Result::failure(error);

		std::lock_guard lock(mutex_);
		auto owner = owners_.find(owner_id);
		if (owner == owners_.end())
			return Result::failure("owner not registered");
		if (is_stale_locked(owner->second))
			return Result::failure("owner is stale; register again");
		const Command *cmd = find_command(owner->second.commands, command);
		if (!cmd)
			return Result::failure("unknown command " + quote_id(command));

		for (auto it = j.begin(); it != j.end(); ++it) {
			const std::pair<std::string, ArgType> *arg = nullptr;
			for (const auto &a : cmd->args) {
				if (a.first == it.key())
					arg = &a;
			}
			if (!arg)
				return Result::failure("unknown argument " + quote_id(it.key()));
			if (!arg_matches(arg->second, it.value()))
				return Result::failure("argument " + quote_id(it.key()) + " must be " +
						       arg_type_name(arg->second));
		}
		return r;
	});
}

Result Registry::heartbeat(std::string_view owner_id)
{
	return guarded([&] {
		Result r = check_owner_arg(owner_id);
		if (!r.ok)
			return r;

		std::lock_guard lock(mutex_);
		auto owner = owners_.find(owner_id);
		if (owner == owners_.end())
			return Result::failure("owner not registered");
		if (is_stale_locked(owner->second))
			return Result::failure("owner is stale; register again");
		owner->second.last_heartbeat = now_();
		return r;
	});
}

bool Registry::is_stale(std::string_view owner_id) const
{
	std::lock_guard lock(mutex_);
	auto owner = owners_.find(owner_id);
	return owner != owners_.end() && is_stale_locked(owner->second);
}

std::size_t Registry::owner_count() const
{
	std::lock_guard lock(mutex_);
	return owners_.size();
}

void Registry::clear()
{
	std::lock_guard lock(mutex_);
	owners_.clear();
}

} // namespace luabridge
