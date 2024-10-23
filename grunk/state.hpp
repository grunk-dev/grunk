#pragma once

#include "object.hpp"
#include "feature.hpp"
#include "action.hpp"
#include <sol/sol.hpp>
#include <stdexcept>

namespace {

sol::object make_dynamic_action(sol::state& lua, sol::protected_function const& func) 
{
    auto decorated_function = [func](sol::variadic_args va) -> grunk::DynamicFeature
    {

        std::vector<grunk::DynamicFeature> args;
        args.reserve(va.size());

        // Use std::transform to convert variadic_args to std::vector<DynamicFeature>
        std::transform(
            va.begin(), va.end(), 
            std::back_inserter(args), 
            [](sol::object const& obj) {
                if (obj.is<grunk::DynamicFeature>()) {
                    return obj.as<grunk::DynamicFeature>();
                } else {
                    return grunk::feature(obj);
                }
            }
        );

        //TODO: For testing only two arguments.
        //Add a dynamic action version, that accepts a vector of DynamicFeatures and 
        //passes them to a sol::protected_function/the eval_and_unwrap lambda?
        return grunk::action(func, args).output();
    };
    return sol::make_object(lua, sol::as_function(decorated_function));
}


} // anonymous namespace 

namespace grunk {

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
    }

    template <typename T, typename... Args>
    void register_type(std::string const& name, Args&&... args) {
        original_env.new_usertype<T>(name, std::forward<Args>(args)...);
    }

    template <typename Func>
    void register_function(std::string const& name, Func&& fun) {
        original_env.set_function(name, std::forward<Func>(fun));

    }

    void set_functions_are_actions(bool use_actions) {
        if (use_actions) {
            active_env[sol::metatable_key]["__index"] = decorated_env;
        } else {
            active_env[sol::metatable_key]["__index"] = original_env;
        }
    }

    template <typename T>
    DynamicFeature feature(T const& value) {
        return grunk::feature(sol::make_object(lua, value));
    }

    template <typename... Args>
    DynamicFeature action(std::string const& function, Args&&... args) {
        sol::protected_function const f = original_env[function];
        return grunk::action(f, std::forward<Args>(args)...).output();
    }

    inline sol::object get(std::string const& key) {
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
            return grunk::feature(obj);
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
            "__add", make_dynamic_action(lua, g["_dynamic_add"]),
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
                            throw std::logic_error(std::string("Construction error: ") + err.what());
                        }
                        sol::object obj = ret[0];
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
