#pragma once

#include "object.hpp"
#include "feature.hpp"
#include "action.hpp"

#include <sol/sol.hpp>
#include <stdexcept>
#include <regex>

namespace {

grunk::object make_dynamic_action(sol::state const& lua, sol::protected_function const& func)
{
    auto decorated_function = [func](sol::variadic_args va) -> grunk::DynamicFeature
    {

        std::vector<grunk::DynamicFeature> args;
        args.reserve(va.size());

        // Use std::transform to convert variadic_args to std::vector<DynamicFeature>
        std::transform(
            va.begin(), va.end(), 
            std::back_inserter(args), 
            [](grunk::object const& obj) {
                if (obj.is<grunk::DynamicFeature>()) {
                    return obj.as<grunk::DynamicFeature>();
                } else {
                    return grunk::feature(obj);
                }
            }
        );

        return grunk::action(func, args).output();
    };
    return sol::make_object(lua, sol::as_function(decorated_function));
}

/**
 * @brief Accesses a nested element in a sol::table by traversing keys separated by dots (.) or colons (:).
 *
 * @param table The sol::table to be traversed.
 * @param str The string representing the path of nested keys, separated by dots (.) or colons (:).
 * @return sol::object The nested object in the table at the specified path.
 *
 * This function splits the string `str` at each dot (.) or colon (:), then uses each part as a key for accessing
 * nested tables in `table`. If any key is invalid or does not exist, an exception is thrown.
 *
 * @note This function assumes the table contains only sol::table elements at each nested level except for the final key.
 * If any key is invalid or does not exist, an exception is thrown.
 */
sol::object lookup_nested(sol::table const& table, std::string const& str) {
    sol::object current = table;

    // Use regex to split by both '.' and ':'
    std::regex delimiter_regex(R"([.:])");
    std::sregex_token_iterator iter(str.begin(), str.end(), delimiter_regex, -1);
    std::sregex_token_iterator end;

    for (; iter != end; ++iter) {
        std::string key = *iter;

        // Ensure current object is a table before accessing the next key
        if (current.get_type() != sol::type::table) {
            std::string error = std::string("Could not resolve \"") + key +
                                "\" in identifier \"" + str +
                                "\". Are all types properly registered in the grunk state?";
            throw std::runtime_error(error);
        }

        current = current.as<sol::table>()[key];
    }

    return current;
}

} // anonymous namespace 

namespace grunk {

// Tag to tell grunk::state::feature to default-construct a type
struct default_construct_t {};
constexpr const default_construct_t default_construct;

class state
{
public:

    inline state()
     : original_env(lua, sol::create)
     , decorated_env(lua, sol::create)
     , active_env(lua, sol::create, decorated_env)
    {
        lua.open_libraries(sol::lib::base);

        // register some basic funcitonality in dynamic type system
        init();

        // manipulates the decorated_env to look up missing symbols in the 
        // original env and decorates the functions as actions
        create_decorated_environment();

        // Initially use decorated_env for symbol lookup
        sol::table active_env_meta = lua.create_table();
        active_env_meta["__index"] = decorated_env;
        active_env[sol::metatable_key] = active_env_meta;

        lua["environments"] = lua.create_table();
        lua["environments"]["original"] = original_env;
        lua["environments"]["decorated"] = decorated_env;
        lua["environments"]["active"] = active_env;
    }

    template <typename T, typename... Args>
    void register_type(std::string const& name, Args&&... args) {
        original_env.new_usertype<T>(name, std::forward<Args>(args)...);
    }

    template <typename Func>
    void register_function(std::string const& name, Func&& fun) {
        original_env.set_function(name, std::forward<Func>(fun));

    }

    void set_functions_are_actions(bool use_actions)
    {
        if (use_actions) {
            active_env[sol::metatable_key]["__index"] = decorated_env;
        } else {
            active_env[sol::metatable_key]["__index"] = original_env;
        }
    }

    sol::table get_type(std::string const& keys_nested) const
    {
        return lookup_nested(original_env, keys_nested);
    }

    sol::protected_function get_function(std::string const& keys_nested) const
    {
        return lookup_nested(original_env, keys_nested);
    }

    template <typename T>
    DynamicFeature feature(T const& value) const
    {
        return grunk::feature(object(sol::make_object(lua, value)));
    }

    DynamicFeature feature(object const& value) const
    {
        return grunk::feature(value);
    }

    template <typename... Args>
    DynamicFeature feature(std::string const& type_name, Args&&... args) const
    {
        sol::table usertype = lookup_nested(original_env, type_name);
        return feature(usertype, std::forward<Args>(args)...);
    }

    DynamicFeature feature(std::string const& type_name, default_construct_t)
    {
        sol::table usertype = lookup_nested(original_env, type_name);
        return feature(usertype);
    }

    template <typename... Args>
    DynamicFeature feature(sol::table usertype, Args&&... args) const
    {
        sol::protected_function const ctor = usertype["new"];
        sol::protected_function_result ret = ctor(std::forward<Args>(args)...);
        if (!ret.valid()) {
            sol::error err = ret;
            throw std::runtime_error(std::string("Construction error: ") + err.what());
        }
        sol::object obj = ret[0];
        return feature(obj);
    }

    template <typename... Args>
    DynamicFeature action(std::string const& function, Args&&... args) const
    {
        sol::protected_function const f = lookup_nested(original_env, function);
        return grunk::action(f, std::forward<Args>(args)...).output();
    }

    template <typename... Args>
    DynamicFeature action(sol::protected_function const& function, Args&&... args) const
    {
        return grunk::action(function, std::forward<Args>(args)...).output();
    }

    inline sol::object operator[](std::string const& key)
    {
        return active_env[key];
    }

    inline DynamicFeature get_feature(std::string const& key)
    {
        DynamicFeature ret = active_env[key];
        return ret;
    }

    inline auto eval(std::string const& lua_script) {
        return lua.script(lua_script, active_env);
    }

private:


    inline void init() {

        auto g = lua.create_named_table("grunk");

        g.set_function("feature", [](sol::object obj) -> DynamicFeature {
            return grunk::feature(object(obj));
        });

        // define some operators dynamically
        lua.script("function grunk._dynamic_add(l, r) return l + r end");
        lua.script("function grunk._dynamic_sub(l, r) return l - r end");
        lua.script("function grunk._dynamic_mul(l, r) return l * r end");
        lua.script("function grunk._dynamic_div(l, r) return l / r end");
        lua.script("function grunk._dynamic_mod(l, r) return l % r end");
        lua.script("function grunk._dynamic_pow(l, r) return l ^ r end");
        lua.script("function grunk._dynamic_unm(v) return -v end");

        // register DynamicFeature as a usertype
        g.new_usertype<DynamicFeature>(
            "Feature",
            sol::constructors<DynamicFeature(sol::object)>(),
            "set_value", &DynamicFeature::set_value<sol::object>,
            "change_value", &DynamicFeature::change_value,
            "with_id", [](DynamicFeature& self, std::string const& v) -> DynamicFeature {
                // need to copy here to allow method chaining at construction in LUA. Example:
                //
                // x = Feature:new(2.):with_id("foo")
                //
                // first creates a temoprary at construction, passes it to with_id which returns a reference.
                // sol is written in such a way, that it doesn't take ownership of references.
                return self.with_id(v);
            },
            "id", &DynamicFeature::id,
            "set_id", &DynamicFeature::set_id,
            "is_valid", &DynamicFeature::is_valid,
            "value", &DynamicFeature::value,
            "compute_node", &DynamicFeature::compute_node,
            "as", static_cast<sol::table(DynamicFeature::*)(sol::table) const>(&DynamicFeature::as),
            "__add", make_dynamic_action(lua, g["_dynamic_add"]), //TODO: Why can't I use &grunk::operator+<DynamicFeature const&, DynamicFeature const&>
            "__sub", make_dynamic_action(lua, g["_dynamic_sub"]),
            "__mul", make_dynamic_action(lua, g["_dynamic_mul"]),
            "__div", make_dynamic_action(lua, g["_dynamic_div"]),
            "__mod", make_dynamic_action(lua, g["_dynamic_mod"]),
            "__pow", make_dynamic_action(lua, g["_dynamic_pow"]),
            "__unm", make_dynamic_action(lua, g["_dynamic_unm"])
        );
    }

    inline void create_decorated_environment() {
    // Create a metatable to intercept function and usertype method lookups
        sol::table mt = lua.create_table();

        // Intercept lookups via __index metamethod
        mt.set_function("__index", [&](sol::table ts, std::string const& key) -> sol::object {

            sol::object result = original_env[key];  // Lookup in original environment

            if (!result.valid()) {
                return lua.globals()[key]; // use globals as fallback, but without decorating callables
            }

            if (result.is<sol::protected_function>()) {
                // Decorate if it's a function
                sol::protected_function func = result.as<sol::protected_function>();
                decorated_env[key] =  make_dynamic_action(lua, func);
                return decorated_env[key];
            } else if (result.is<sol::table>()) {
                // If it's a usertype (stored as a table), intercept its metatable

                sol::table usertype_table = result.as<sol::table>();

                sol::table decorated_table = lua.create_table();
                sol::table decorated_table_meta = lua.create_table();
                decorated_table_meta.set_function("__index", [this, usertype_table](sol::table ts, std::string const& key) -> sol::object {

                    sol::object method = usertype_table[key];  // Lookup method in original metatable

                    if (method.is<sol::protected_function>()) {
                        // Decorate methods
                        sol::protected_function func = method.as<sol::protected_function>();
                        return make_dynamic_action(lua, func);
                    }

                    return method;  // Return non-function elements as-is
                });
                decorated_table[sol::metatable_key] = decorated_table_meta;

                // intercept constructors to add a new_feature method
                if (usertype_table["new"].valid()) {
                    sol::protected_function ctor = usertype_table["new"];
                    decorated_table["new_feature"] = [ctor](sol::variadic_args args) -> DynamicFeature {
                        sol::protected_function_result ret = ctor(args);
                        if (!ret.valid()) {
                            sol::error err = ret;
                            throw std::runtime_error(std::string("Construction error: ") + err.what());
                        }
                        object obj = ret[0];
                        return grunk::feature(obj);
                    };
                }

                decorated_env[key] = sol::make_object(lua, decorated_table);
                return decorated_env[key];
            }

            decorated_env[key] = result;
            return decorated_env[key];  // Return non-function, non-usertype results as-is
        });

        // Set the decorated metatable on the new environment
        decorated_env[sol::metatable_key] = mt;
    }

    sol::state lua;
    sol::environment original_env;
    sol::environment decorated_env;
    sol::environment active_env;
};

} // namespace grunk
