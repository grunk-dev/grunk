#pragma once

#include "object.hpp"
#include "feature.hpp"
#include "internal/sol_helpers.hpp"
#include "internal/usertype_proxy.hpp"
#include "function_meta.hpp"
#include "action.hpp"
#include "environment.hpp"

#include <sol/sol.hpp>
#include <stdexcept>

namespace grunk {

// Tag to tell grunk::state::feature to default-construct a type
struct default_construct_t {};
constexpr const default_construct_t default_construct;

/**
 * @brief The state class is responsible for grunk's dynamic scripting capabilities and
 * LUA interface. **Important:** A grunk instance must outlive any dynamic feature created
 * from it!
 *
 * Use this class to register types and functions at runtime for use in a grunk recipe, instantiate
 * dynamic features and actions and running LUA scripts.
 *
 * The state holds the LUA state as well as three environments, the original environment,
 * the decorated environment and the active environment. The active environment is where variables
 * are created when executing dynamic scripts. When running code in the active environment, symbols
 * are looked up either in the decorated environment (default) or the original environment.
 *
 * The original environment is where registered types and functions are stored. The decorated environment
 * has access to all symbols in the original environment, but every function and method is decorated as an
 * action. Therefore, any code executed in the decorated environment will have dependency tracking, lazy
 * evaluation and automatic invalidation enabled. Code run in the original environment simply uses the original
 * undecorated symbols.
 *
 */
class state
{
public:

    /**
     * @brief Creates a grunk::state and initializes the environments. It also registers some
     * grunk symbols which can be used in a LUA script
     */
    inline state()
     : original_env(lua, sol::create)
     , decorated_env(lua, sol::create)
    {
        lua.open_libraries(sol::lib::base);

        // register grunk symbols in dynamic type system
        init();

        // manipulates the decorated_env to look up missing symbols in the 
        // original env and decorates the functions as actions
        create_decorated_environment();

        lua["environments"] = lua.create_table();
        lua["environments"]["original"] = original_env;
        lua["environments"]["decorated"] = decorated_env;
    }

    /**
     * @brief register_type registers a type in the dynamic type system.
     *
     * This type is stored as a symbol in the original environment by default. Optionally,
     * a sol::table can be specified. If it is specified, the type will be registered inside the
     * sol::table. Note that only symbols stored in the original environment can be decorated.
     *
     * @param name Name of the type
     * @param table optional table as a "namespace", where the type shall be registered.
     * @returns a usertype_proxy<T> to allow method chaining
     */
    template <typename T>
    auto register_type(std::string const& name, std::optional<sol::table> table = std::nullopt)
    {
        if (!table) {
            table = original_env;
        }
        return usertype_proxy<T>{name, table->new_usertype<T>(name)};
    }

    /**
     * @brief register_function registers a function in the dynamic type system.
     *
     * This function is stored as a symbol in the original environment by default. Optionally,
     * a sol::table can be specified. It it is specified, the type will be registered inside the
     * sol::table. Note that only symobls stored in the original environment can be decorated.
     *
     * @param name Name of the function
     * @param fun The function to be registered. This can be a function pointer, a functor or a
     *            lambda expression
     * @param table optional table as a "namespace", where the type shall be registered.
     */
    template <typename Func>
    void register_function(std::string const& name, Func&& fun, std::vector<Parameter> params = {}, std::optional<sol::table> table = std::nullopt)
    {
        // set function

        if (!table) {
            table = original_env;
        }
        auto meta_func = create_function_meta(lua, name, params, std::forward<Func>(fun));
        table->set(name, meta_func);
    }

    environment create_env() const {
        sol::environment env(lua, sol::create, lua.globals()); 
        env[sol::metatable_key]["__index"] = original_env;
        return environment(env);
    }

    environment create_parametric_env() const {
        sol::environment env(lua, sol::create, lua.globals()); 
        env[sol::metatable_key]["__index"] = decorated_env;
        return environment(env);
    }

    /**
     * @brief get_type returns a type based on a nested string of keys. The key is assumed to be seperated
     * using either dots (.) or colons (:). get_type("foo.bar.baz") would look up original_env["foo"]["bar"]["baz"].
     * @param keys_nested A string of nested keys
     * @return A sol::table representing the type
     */
    sol::table get_type(std::string const& keys_nested) const
    {
        return details::lookup_nested(original_env, keys_nested);
    }

    /**
     * @brief get_function returns a function based on a nested string of keys. The key is assumed to be
     * seperated using either dots (.) or colons (:). get_function("foo.bar:baz") would lookup
     * original_env["foo"]["bar"]["baz"].
     * @param keys_nested A string of nested keys
     * @return A sol::protected_function
     */
    function_meta get_function(std::string const& keys_nested) const
    {
        sol::object tmp = details::lookup_nested(original_env, keys_nested);
        if (!tmp.is<function_meta>()) {
            throw std::logic_error("Function \"" + keys_nested + "\" is not registered in the dynamic function registry.");
        }
        return tmp.as<function_meta>();
    }

    /**
     * @brief feature Creates a new dynamic feature wrapping a value
     * @param value The value to be wrapped
     * @return  a DynamicFeature instance
     */
    template <typename T>
    DynamicFeature feature(T const& value) const
    {
        return grunk::feature(object(sol::make_object(lua, value)));
    }

    /**
     * @brief feature Creates a new dynamic feature wrapping an existing
     * grunk::object
     * @param value The grunk::object
     * @return a DynamicFeature instance
     */
    DynamicFeature feature(object const& value) const
    {
        return grunk::feature(value);
    }

    /**
     * @brief feature Creates a new dynamic feature by invoking the registered
     * constructor/new-method with the provided constructor arguments
     * @param type_name The name of the type
     * @param args The constructor arguments
     * @return a DynamicFeature instance
     */
    template <typename... Args>
    DynamicFeature feature(std::string const& type_name, Args&&... args) const
    {
        sol::table usertype = get_type(type_name);
        return feature(usertype, std::forward<Args>(args)...);
    }

    /**
     * @brief feature Creates a new dynamic feature by invoking the default
     * constructor of the type, if it exists.
     *
     * This function uses a tag-dispatch method to avoid ambiguity with the templated
     * grunk::feature method that constructs a DynamicFeature wrapping a string.
     *
     * Use it like this: auto x = grunk.feature("foo", grunk::default_construct);
     *
     * @param type_name The name of the type
     * @param default_construct The default_construct tag.
     * @return A DynamicFeature instance
     */
    DynamicFeature feature(std::string const& type_name, default_construct_t default_construct)
    {
        sol::table usertype = get_type(type_name);
        return feature(usertype);
    }

    /**
     * @brief feature Creates a new dynamic feature by invoking the registered
     * constructor/new-method with the provided constructor arguments
     * @param usertype A sol::table representing the type
     * @param args The constructor arguments
     * @return a DynamicFeature instance
     * @return
     */
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

    /**
     * @brief action Creates an action representing the function evaluation given
     * the passed arguments. This will internally register this computation in the
     * underlying dependency graph.
     *
     * If one of the arguments is not yet a Feature instance, an anonymous Feature
     * (i.e. a constant) will be created on the fly.
     *
     * @param function The name of the function to be evaluated
     * @param args The arguments passed to the function.
     * @return a DynamicFeature instance representing the calculation result
     */
    template <typename... Args>
    DynamicFeature action(std::string const& function, Args&&... args) const
    {
        sol::object const f = details::lookup_nested(original_env, function);
        if (!f.is<function_meta>()) {
            throw std::logic_error("Function \"" + function + "\" is not registered in the dynamic function registry.");
        }
        return grunk::action(f.as<function_meta>(), std::forward<Args>(args)...).output();
    }

    /**
     * @brief action Creates an action representing the function evaluation given
     * the passed arguments. This will internally register this computation in the
     * underlying dependency graph.
     *
     * If one of the arguments is not yet a Feature instance, an anonymous Feature
     * (i.e. a constant) will be created on the fly.
     * @param function  The function to be evaluated
     * @param args The arguments passed to the function.
     * @return a DynamicFeature instance representing the calculation result
     */
    template <typename... Args>
    DynamicFeature action(function_meta const& function, Args&&... args) const
    {
        return grunk::action(function, std::forward<Args>(args)...).output();
    }

    /**
     * @brief deserializes a string back to a grunk::object
     *
     * This assumes thtat the object has been previously serialized
     * with grunk::serialize
     */
    inline grunk::object deserialize(std::string const& v)
    {
        auto ret = lua.script("return " + v, original_env);
        if (ret.valid()) {
            return ret;
        } else {
            throw io_error("Error deserializing \"" + v + "\".");
        }
    }

private:


    /**
     * @brief init registers some important grunk functionality in the dynamic type system,
     * specifically functionality around grunk::DynamicFeature
     */
    inline void init() {

        // create internal table used by grunk itself.
        auto g = lua.create_named_table("grunk");

        g.new_usertype<function_meta>("function_meta",
            sol::no_constructor,
            sol::meta_function::call, &function_meta::operator()
        );

        
        register_function(
            "feature",
            [](sol::object obj) -> DynamicFeature {
                return grunk::feature(obj);
            },
            {Parameter{"object", }},
            g
        );

        // define some operators dynamically
        lua.script("function grunk.__dynamic_add(l, r) return l + r end");
        sol::protected_function _addfun = g["__dynamic_add"];
        g["_dynamic_add"] = create_function_meta(
            lua, 
            "grunk._dynamic_add", 
            {Parameter{"lhs", }, Parameter{"rhs",}},
            _addfun
        );
        function_meta const& _add = g["_dynamic_add"];

        lua.script("function grunk.__dynamic_sub(l, r) return l - r end");
        sol::protected_function _subfun = g["__dynamic_sub"];
        g["_dynamic_sub"] = create_function_meta(
            lua, 
            "grunk._dynamic_sub", 
            {Parameter{"lhs", }, Parameter{"rhs",}},
            _subfun
        );
        function_meta const& _sub = g["_dynamic_sub"];

        lua.script("function grunk.__dynamic_mul(l, r) return l * r end");
        sol::protected_function _mulfun = g["__dynamic_mul"];
        g["_dynamic_mul"] = create_function_meta(
            lua, 
            "grunk._dynamic_mul", 
            {Parameter{"lhs", }, Parameter{"rhs",}},
            _mulfun
        );
        function_meta const& _mul = g["_dynamic_mul"];

        lua.script("function grunk.__dynamic_div(l, r) return l / r end");
        sol::protected_function _divfun = g["__dynamic_div"];
        g["_dynamic_div"] = create_function_meta(
            lua, 
            "grunk._dynamic_div", 
            {Parameter{"lhs", }, Parameter{"rhs",}},
            _divfun
        );
        function_meta const& _div = g["_dynamic_div"];

        lua.script("function grunk.__dynamic_mod(l, r) return l % r end");
        sol::protected_function _modfun = g["__dynamic_mod"];
        g["_dynamic_mod"] = create_function_meta(
            lua, 
            "grunk._dynamic_mod", 
            {Parameter{"lhs", }, Parameter{"rhs",}},
            _modfun
        );
        function_meta const& _mod = g["_dynamic_mod"];

        lua.script("function grunk.__dynamic_pow(l, r) return l ^ r end");
        sol::protected_function _powfun = g["__dynamic_pow"];
        g["_dynamic_pow"] = create_function_meta(
            lua, 
            "grunk._dynamic_pow", 
            {Parameter{"base", }, Parameter{"exponent",}},
            _powfun
        );
        function_meta const& _pow = g["_dynamic_pow"];

        lua.script("function grunk.__dynamic_unm(v) return -v end");
        sol::protected_function _unmfun = g["__dynamic_unm"];
        g["_dynamic_unm"] = create_function_meta(
            lua, 
            "grunk._dynamic_unm", 
            {Parameter{"value", }},
            _unmfun
        );
        function_meta const& _unm = g["_dynamic_unm"];

        // register DynamicFeature (and its base classes) as a usertype
        register_type<parametric::param<object>>("param", g)
        .add_member_function("value", &parametric::param<object>::value, {})
        .add_member_function("set_value", &parametric::param<object>::set_value, {Parameter{"value", }})
        .add_member_function("change_value", &parametric::param<object>::change_value, {});

        using DynamicFeatureBase = FeatureBase<DynamicFeature, object>;
        register_type<DynamicFeatureBase>("FeatureBase", g)
        .add_bases<parametric::param<object>>();

        register_type<DynamicFeature>("Feature", g)
        .add_bases<DynamicFeatureBase, parametric::param<object>>()
        .add_constructors(
            [](sol::object obj) -> DynamicFeature {
                return grunk::feature(obj);
            }
        )
        .add_member_function("set_value", &DynamicFeature::set_value<sol::object>, {Parameter{"value", }})
        .add_member_function("change_value", &DynamicFeature::change_value, {})
        .add_member_function(
            "with_id",
            [](DynamicFeature& self, std::string const& v) -> DynamicFeature {
                // need to copy here to allow method chaining at construction in LUA. Example:
                //
                // x = Feature:new(2.):with_id("foo")
                //
                // first creates a temoprary at construction, passes it to with_id which returns a reference.
                // sol is written in such a way, that it doesn't take ownership of references.
                return self.with_id(v);
            },
            {Parameter{"id", }}
        )
        .add_member_function("id", &DynamicFeature::id, {})
        .add_member_function("set_id", &DynamicFeature::set_id, {Parameter{"id", }})
        .add_member_function("is_valid", &DynamicFeature::is_valid, {})
        .add_member_function("value", &DynamicFeature::value, {})
        .add_member_function("compute_node", &DynamicFeature::compute_node, {})
        .add_member_function("as", static_cast<sol::table(DynamicFeature::*)(sol::table) const>(&DynamicFeature::as), {Parameter{"usertype", }})
        .add_member_function(sol::meta_function::addition, details::make_dynamic_action(lua, _add), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::subtraction, details::make_dynamic_action(lua, _sub), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::multiplication, details::make_dynamic_action(lua, _mul), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::division, details::make_dynamic_action(lua, _div), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::modulus, details::make_dynamic_action(lua, _mod), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::power_of, details::make_dynamic_action(lua, _pow), {Parameter{"base", }, Parameter{"exponent",}  })
        .add_member_function(sol::meta_function::unary_minus, details::make_dynamic_action(lua, _unm), {Parameter{"value",}  });

    }

    /**
     * @brief create_decorated_environment sets up the lookup mechanism as well as the lazy decoration of the decorated environment.
     *
     * Whenever a symbol is looked up in the decorated environment and not found, the key will be searched in the original environment.
     * If it is found and is a method, it will be decorated as an action, stored in the decorated environment and returned. If it is not
     * a function, the symbol will be stored as is in the decorated environment.
     */
    inline void create_decorated_environment() {
    // Create a metatable to intercept function and usertype method lookups
        sol::table mt = lua.create_table();

        // Intercept lookups via __index metamethod
        mt.set_function("__index", [&](sol::table ts, std::string const& key) -> sol::object {

            sol::object result = original_env[key];  // Lookup in original environment

            if (!result.valid()) {
                return lua.globals()[key]; // use globals as fallback, but without decorating callables
            }

            if (result.is<function_meta>()) {
                // Decorate if it's a function
                function_meta func = result.as<function_meta>();
                decorated_env.set_function(key, details::make_dynamic_action(lua, func));
                return decorated_env[key];
            } else if (result.is<sol::table>()) {
                // If it's a usertype (stored as a table), intercept its metatable

                sol::table usertype_table = result.as<sol::table>();

                sol::table decorated_table = lua.create_table();
                sol::table decorated_table_meta = lua.create_table();
                decorated_table_meta.set_function("__index", [this, usertype_table](sol::table ts, std::string const& key) -> sol::object {

                    sol::object method = usertype_table[key];  // Lookup method in original metatable

                    if (method.is<function_meta>()) {
                        // Decorate methods
                        return sol::make_object(lua, details::make_dynamic_action(lua, method.as<function_meta>()));
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
};

} // namespace grunk
