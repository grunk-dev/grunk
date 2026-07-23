// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include "ModuleAction.hpp"

#include <sol/sol.hpp>

namespace grunk {

    void ModuleAction::eval() const
    {
        std::string const& current_script = script().value();

        if (!cached_script || *cached_script != current_script) {

            sol::state_view lua_view(lua);
            sol::table original_env = lua_view["grunk"]["env"];

            // A fresh module table is compiled on every script change. Its metatable falls
            // back to the original (undecorated) environment, so the module's own code can
            // reference registered types and functions, as well as other modules.
            sol::table module = lua_view.create_table();
            sol::table mt = lua_view.create_table();
            mt["__index"] = original_env;
            module[sol::metatable_key] = mt;

            // Wrap the module table itself as the execution environment, so that functions
            // defined by the script are stored directly in the module table.
            sol::environment exec_env(lua_view, module);

            sol::protected_function_result res = lua_view.script(current_script, exec_env);
            if (!res.valid()) {
                sol::error err = res;
                throw std::runtime_error(
                    "Error compiling module \"" + module_name + "\": " + err.what()
                );
            }

            compiled_module = module;
            cached_script = current_script;
        }

        sol::object fn_obj = compiled_module.raw_get<sol::object>(function_name);
        if (!fn_obj.valid() || fn_obj.get_type() != sol::type::function) {
            throw std::runtime_error(
                "Function \"" + function_name + "\" is not defined in module \"" + module_name + "\"."
            );
        }
        sol::protected_function fn = fn_obj.as<sol::protected_function>();

        std::vector<object> inputs_vec;
        inputs_vec.reserve(this->num_parents() - 1);
        for (size_t i = 1; i < this->num_parents(); ++i) {
            inputs_vec.push_back(argument(static_cast<int>(i)).value());
        }

        sol::protected_function_result call_res = fn(sol::as_args(inputs_vec));
        if (!call_res.valid()) {
            sol::error err = call_res;
            throw std::runtime_error(
                "Error evaluating module function \"" + module_name + "." + function_name + "\": " + err.what()
            );
        }

        if (auto output = result(); output) {
            output->set_value(call_res[0]);
        }
    }

    std::string ModuleAction::serialize() const
    {
        std::string ret;
        if (auto const& output = result(); output) {
            if (!output->id().empty()) {
                ret += output->id() + " = ";
            }
        } else {
            return "";
        }

        ret += module_name + "." + function_name + "(";

        auto serialize_arg = [](parametric::DAGNode const& node) -> std::string
        {
            bool is_anonymous = (node.id() == "");
            bool is_constant = (node.num_parents() == 0 && is_anonymous);
            if (is_constant) {
                return node.serialize();
            } else if (is_anonymous) {
                return node.get_parents()[0]->serialize();
            } else {
                return node.id();
            }
        };

        bool first_arg = true;
        for (size_t i = 1; i < this->num_parents(); ++i) {
            if (first_arg) {
                first_arg = false;
            } else {
                ret += ", ";
            }
            ret += serialize_arg(*this->get_parents()[i]);
        }
        ret += ")";

        return ret;
    }

} // namespace grunk
