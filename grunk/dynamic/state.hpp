// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "object.hpp"
#include "DynamicFeature.hpp"
#include "sol_helpers.hpp"
#include "usertype_proxy.hpp"
#include "external_type_proxy.hpp"
#include "function_meta.hpp"
#include "ActionDynamic.hpp"
#include "environment.hpp"

#ifdef GRUNK_WITH_RECIPE
#include "grunk/recipe/Recipe.hpp"
#include "grunk/recipe/RecipeCaller.hpp"
#endif

#include <sol/sol.hpp>

#include <stdexcept>
#include <fstream>

namespace grunk {

// Tag to tell grunk::state::feature to default-construct a type
struct default_construct_t {};
constexpr const default_construct_t default_construct;

/**
 * @brief Identifies a plugin registered with a grunk::state: a name (also used as its
 * namespace table's key in the original environment) and a version string.
 *
 * This is the one piece every kind of grunk plugin is meant to share, whatever
 * mechanism it uses to actually populate its namespace - a compiled Lua module (see
 * state::load_compiled_plugin), a Lua script (candidate: layering this over
 * run_module_script/run_module_file), or plain C++ calling register_type/
 * register_function directly against the table returned by a future
 * state::begin_plugin(info). Only the compiled-module path is implemented so far.
 *
 * @ingroup dynamic
 */
struct PluginInfo
{
    std::string name;
    std::string version;
};

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
 * @ingroup dynamic
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

        // create internal table used by grunk itself.
        auto g = lua.create_named_table("grunk");

        // register grunk symbols in dynamic type system
        init();

        // manipulates the decorated_env to look up missing symbols in the 
        // original env and decorates the functions as actions
        create_decorated_environment();

        lua["grunk"]["env"] = original_env;
        lua["grunk"]["parametric_env"] = decorated_env;
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
    template <typename T, sol::automagic_flags Flags = sol::automagic_flags::all>
    auto register_type(std::string const& name, std::optional<sol::table> table = std::nullopt)
    {
        if (!table) {
            table = original_env;
        }
        return usertype_proxy<T>{name, table->new_usertype<T>(name, sol::constant_automagic_enrollments<Flags>{})};
    }

    /**
     * @brief modify_type extends an already-registered usertype with additional members.
     *
     * Unlike `register_type`, this does NOT call `sol::new_usertype` — it looks up an
     * existing usertype table in the Lua state and wraps it in a `sol::usertype<T>`.
     * This avoids sol2's automagic registration entirely (comparison operators, index
     * metamethods, etc.), which is useful for types with `unique_usertype_traits` where
     * the underlying type lacks operators.
     *
     * The usertype must already exist in the Lua state (e.g. registered by a plugin
     * before `modify_type` is called).
     *
     * @param name Name of the existing type
     * @param table optional table as a "namespace" where the type is registered
     * @returns a usertype_proxy<T> to allow method chaining (e.g. `.with_std_vector()`)
     */
    template <typename T>
    auto modify_type(std::string const& name, std::optional<sol::table> table = std::nullopt)
    {
        if (!table) {
            table = original_env;
        }
        lua_State* L = lua;
        int table_idx = (*table).push(L);
        sol::usertype<T> ut(L, table_idx);
        lua_pop(L, 1);
        return usertype_proxy<T>{name, ut};
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
     * @param params optional metadata for the function parameters
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


    /**
     * @brief create_env creates a new environment based on the original environment.
     *
     * A new environment is created with the original environment as its __index metamethod, which means that any code executed in this environment will have access to all symbols in the original environment, but any new symbols created in this environment will not be visible in the original environment. This is the environment that should be used for executing user-provided LUA scripts, where you don't want dependency tracking, lazy evaluation and automatic invalidation to be enabled.
     *
     * @return A new environment instance.
     */
    inline environment create_env() const {
        sol::environment env(lua, sol::create, lua.globals()); 
        env[sol::metatable_key]["__index"] = original_env;
        return environment(env);
    }

    /**
     * @brief create_parametric_env creates a new parametric environment based on the decorated environment.
     *
     * A parametric environment is an environment where all functions are decorated as actions, which means that any code executed in this environment will have dependency tracking, lazy evaluation and automatic invalidation enabled. This is the environment that should be used for executing grunk recipes and for creating grunk features and actions.
     *
     * @return A new parametric environment instance.
     */
    inline environment create_parametric_env() const {
        sol::environment env(lua, sol::create, lua.globals());
        env[sol::metatable_key]["__index"] = decorated_env;
        return environment(env);
    }

    /**
     * @brief run_module_script executes a Lua script, storing every symbol it defines in a
     * named module table in the original environment.
     *
     * If a module with the given @p name already exists, the existing table is reused and the
     * script is executed against it, so multiple scripts/files can incrementally populate the
     * same module. The module table's lookups fall back to the original environment, so the
     * script can reference any other registered type, function or module.
     *
     * Functions defined by the script become available both in a plain environment (as regular,
     * uninstrumented Lua functions - see create_env) and in a parametric environment, where they
     * are decorated into actions like any other symbol registered in the original environment
     * (see create_parametric_env). Statements *within* a module function are not themselves
     * parametric, which is what allows module functions to use non-const setters to build up
     * objects - see the "Modules" section of the documentation.
     *
     * @param name name of the module to create or populate in the original environment
     * @param script the Lua source code to execute
     * @note Lua syntax errors in the script are not detected at registration time.
     *       They surface the first time a function from the module is executed
     *       (e.g. via ``env.eval(...)`` or ``grunk.action(...)``).  This is intentional:
     *       the script is stored as a ``Feature<std::string>`` so that editing it
     *       invalidates every downstream compute node that calls into the module.
     */
    inline void run_module_script(std::string const& name, std::string const& script)
    {
        sol::object existing = original_env[name];
        sol::table module = (existing.valid() && existing.is<sol::table>())
            ? existing.as<sol::table>()
            : create_module(name);

        // Wrap the module table itself as the execution environment, so that symbols defined
        // by the script are stored directly in the module table. Its metatable's __index
        // (set up in create_module) makes the original environment available as a fallback.
        sol::environment exec_env(lua, module);

        sol::protected_function_result res = lua.script(script, exec_env);
        if (!res.valid()) {
            sol::error err = res;
            throw std::runtime_error(std::string("Error running module script \"") + name + "\": " + err.what());
        }

        decorate_module_functions(module);
    }

    /**
     * @brief run_module_file loads a Lua file from disk and runs it into a module, following
     * the same semantics as run_module_script.
     *
     * @param name name of the module to create or populate
     * @param filename path to a Lua file
     */
    inline void run_module_file(std::string const& name, std::string const& filename)
    {
        std::ifstream fin(filename);
        if (!fin) {
            throw io_error("Could not open module file: " + filename);
        }
        std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        run_module_script(name, content);
    }

    /**
     * @brief load_module loads a compiled Lua C extension (e.g. a SWIG-Lua module) into
     * this state's Lua interpreter, following Lua's own loading convention (the same one
     * `require` uses).
     *
     * This is how a plugin built as a native shared library gets pulled into grunk, as
     * opposed to run_module_script/run_module_file, which populate a module table by
     * running Lua source. The returned table is the module's own content (e.g. classes
     * and free functions) - it is not yet integrated into grunk's dynamic type system.
     * Use decorate_module_functions to make its plain functions grunk-trackable, and
     * register_external_type to bridge classes whose constructor/methods aren't reachable
     * as plain table entries (see external_type_proxy for why that's needed).
     *
     * @param name the module name, used as the registry key so repeated loads of the
     *             same name return the already-loaded module instead of reinitializing it
     * @param open_fn the module's C entry point (e.g. `luaopen_occt`)
     * @param set_global if true, also assign the module table to a global of the same name
     * @return the module's table, as returned by open_fn
     */
    inline sol::table load_module(std::string const& name, lua_CFunction open_fn, bool set_global = false)
    {
        lua_State* L = lua;
        luaL_requiref(L, name.c_str(), open_fn, set_global ? 1 : 0);
        sol::table module = sol::stack::pop<sol::table>(L);
        return module;
    }

    /**
     * @brief decorate_module_functions converts every plain Lua function stored in a
     * table (recursively, for nested tables) into a function_meta, so that the generic
     * decoration logic in create_decorated_environment recognizes and wraps them as
     * actions when accessed through a parametric environment. Already-converted entries
     * (function_meta userdata) are left untouched, so this is safe to call repeatedly.
     *
     * This is what run_module_script/run_module_file use internally to decorate scripted
     * modules; it is equally useful for a table loaded via load_module (e.g. the free
     * functions of a SWIG-Lua module), since those arrive as plain sol::protected_function
     * values, not function_meta.
     *
     * Note this only reaches functions stored directly as table entries. It will not find
     * methods that are only reachable through an instance's own metatable (as is typical
     * for classes in a SWIG-Lua "OOP" style binding) - see register_external_type for
     * bridging those.
     *
     * @param table the table to decorate
     */
    inline void decorate_module_functions(sol::table table)
    {
        for (auto& kv : table) {
            sol::object key = kv.first;
            sol::object value = kv.second;

            if (!key.is<std::string>()) {
                continue;
            }

            if (value.is<sol::table>()) {
                decorate_module_functions(value.as<sol::table>());
            } else if (value.get_type() == sol::type::function) {
                std::string const function_name = key.as<std::string>();
                sol::protected_function func = value.as<sol::protected_function>();
                table.set(function_name, create_function_meta(lua, function_name, {}, func));
            }
        }
    }

    /**
     * @brief load_compiled_plugin loads a compiled Lua C extension (e.g. a SWIG-Lua
     * module) as a grunk plugin: its own table is registered as a namespace under its
     * name in the original environment, its free functions are made grunk-trackable,
     * and its identity is recorded in plugins().
     *
     * Classes the module exposes (e.g. adtl.adouble) are not yet usable as grunk types
     * after this call - their constructor/methods aren't plain table entries (see
     * external_type_proxy for why), so they still need bridging via
     * register_external_type, passing this method's return value as that call's
     * `table` argument and `<name>.<ClassName>` as its `name`, so the class ends up
     * reachable at the same path it was loaded under.
     *
     * @param info the plugin's name and version. `info.name` doubles as the module's
     *             loading key (see load_module) and thus must match the module's own
     *             internal identity (e.g. SWIG's `%module` name).
     * @param open_fn the module's C entry point (e.g. `luaopen_adtl`)
     * @return the plugin's namespace table (the module's own table, decorated so its
     *         free functions are grunk-tracked)
     */
    inline sol::table load_compiled_plugin(PluginInfo const& info, lua_CFunction open_fn)
    {
        sol::table ns = load_module(info.name, open_fn);
        decorate_module_functions(ns);
        original_env.set(info.name, ns);
        m_plugins.push_back(info);
        return ns;
    }

    /**
     * @brief plugins returns the name and version of every plugin loaded so far via one
     * of the load_*_plugin methods.
     */
    inline std::vector<PluginInfo> const& plugins() const
    {
        return m_plugins;
    }

    /**
     * @brief register_external_type registers a type that already lives in Lua (e.g. a
     * class exposed by a module loaded via load_module) as a grunk dynamic type, without
     * requiring a compile-time C++ type. See external_type_proxy for the mechanism and
     * its limitations.
     *
     * @param name the type's fully-qualified display name, i.e. the Lua expression that
     *             reaches it - "adouble" if registered flat in original_env, "adtl.adouble"
     *             if registered nested inside a plugin/namespace table named "adtl". This
     *             is what function_meta identifiers are built from, so it must match how
     *             the type is actually reached: an action's serialized form (see
     *             ActionDynamic::serialize) embeds it verbatim, and that text must
     *             resolve correctly when a saved recipe is read back in.
     * @param ctor the type's constructor, callable as ctor(args...) - e.g. the "static"
     *             table a SWIG-Lua class is exposed as, whose __call metamethod
     *             constructs instances
     * @param table optional table as a "namespace" where the type shall be registered,
     *              under the last dot-separated segment of `name`. Defaults to the
     *              original environment, exactly like register_type.
     * @param probe optional pre-built instance of the type, used to discover methods via
     *              add_member_function. If omitted, one is lazily constructed by calling
     *              ctor with no arguments the first time a method is bridged.
     * @returns an external_type_proxy to allow method chaining
     */
    inline external_type_proxy register_external_type(
        std::string const& name,
        sol::protected_function ctor,
        std::optional<sol::table> table = std::nullopt,
        sol::object probe = sol::lua_nil)
    {
        if (!table) {
            table = original_env;
        }
        // The insertion key is just the type's own name (the last segment of a
        // dotted `name`) - the rest of `name` is only the namespace it is meant to
        // already be reachable through via `table`.
        std::string key = name;
        if (auto pos = name.rfind('.'); pos != std::string::npos) {
            key = name.substr(pos + 1);
        }

        // Mirrors what sol2's new_usertype does for register_type: create a fresh table
        // to represent the type itself, and register it under `key` in the namespace
        // table, so decorated code can find it via table[key].
        sol::table type_table = lua.create_table();
        table->set(key, type_table);
        return external_type_proxy{name, lua, ctor, type_table, probe};
    }

    /**
     * @brief clear_module removes a module table from the original and decorated environments.
     *
     * Use this to force a clean reload of a module, e.g. `clear_module(name)` followed by
     * `run_module_script(name, new_script)`, rather than relying on run_module_script's
     * incremental-augmentation behavior.
     *
     * @param name name of the module to remove
     */
    inline void clear_module(std::string const& name)
    {
        original_env.set(name, sol::lua_nil);
        decorated_env.set(name, sol::lua_nil);
    }

#ifdef GRUNK_WITH_RECIPE

    /**
     * @brief create_recipe creates a new recipe based on the decorated environment.
     *
     * A recipe is created with a parametric environment, which means that any code executed in this recipe will have dependency tracking, lazy evaluation and automatic invalidation enabled. This is the environment that should be used for executing grunk recipes and for creating grunk features and actions.
     *
     * @return A new recipe instance.
     */
    inline Recipe create_recipe() const {
        return Recipe(create_parametric_env());
    }

    /**
     * @brief write writes a recipe to a file. The recipe is serialized using the to_string method of the recipe, which returns a LUA script that can be executed to recreate the recipe.
     *
     * @param filename The name of the file to write the recipe to
     * @param recipe The recipe to be written to the file
     */
    inline void write(std::string const& filename, Recipe const& recipe)
    {
        std::ofstream fout(filename);
        fout << recipe.to_string() << "\n";
    }

    /**
     * @brief read reads a recipe from a file.
     *
     * @param filename The name of the file to read the recipe from
     * @return A new recipe instance
     */
    inline Recipe read(std::string const& filename) {
        auto recipe = create_recipe();
        recipe.populate_from_file(filename);
        return recipe;
    }
#endif

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
     * @brief feature Creates a new dynamic feature without an initial value. This can be used as a placeholder for a value that will be set later, e.g. when creating a recipe with some features that are not yet known.
     * @return a DynamicFeature instance
     */
    inline DynamicFeature feature() const
    {
        return DynamicFeature(original_env.lua_state());
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
     * @brief object Creates a grunk::object from a value. This is useful for wrapping values in a grunk::object without creating a DynamicFeature, e.g. when passing arguments to an action that are not features themselves.
     * @param value The value to be wrapped
     * @return a grunk::object instance
     */
    template <typename T>
    grunk::object create_object(T const& value) const
    {
        return sol::make_object(lua, value);
    }

    /**
     * @brief feature Creates a new dynamic feature wrapping an existing
     * grunk::object
     * @param value The grunk::object
     * @return a DynamicFeature instance
     */
    DynamicFeature feature(grunk::object const& value) const
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
        return grunk::action(f.as<function_meta>(), std::forward<Args>(args)...);
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
        return grunk::action(function, std::forward<Args>(args)...);
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

        sol::table g = lua["grunk"];

        g.new_usertype<function_meta>("function_meta",
            sol::no_constructor,
            sol::meta_function::call, &function_meta::operator()
        );

        
        register_function(
            "feature",
            sol::overload(
                [=](sol::object obj) -> DynamicFeature {
                    return feature(obj);
                },
                [=]() -> DynamicFeature {
                    return feature();
                }
            ),
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

        register_type<DynamicFeature>("Feature", g)
        .add_constructors(
            [](sol::object obj) -> DynamicFeature {
                return grunk::feature(obj);
            }
        )
        .add_member_function("value", [](DynamicFeature const& f) { return f.value(); }, {})
        .add_member_function("set_value", [](DynamicFeature& f, grunk::object const& v){ return f.set_value(v); }, {Parameter{"value", }})
        .add_member_function("change_value", [](DynamicFeature& f) { return f.change_value(); }, {})
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
        .add_member_function("id", [](DynamicFeature const& f) { return f.id(); }, {})
        .add_member_function("set_id", [](DynamicFeature& f, std::string const& id){ return f.set_id(id); }, {Parameter{"id", }})
        .add_member_function("is_valid", &DynamicFeature::is_valid, {})
        .add_member_function("compute_node", &DynamicFeature::compute_node, {})
        .add_member_function("as", static_cast<sol::table(DynamicFeature::*)(sol::table) const>(&DynamicFeature::as), {Parameter{"usertype", }})
        .add_member_function(sol::meta_function::addition, details::make_dynamic_action(_add), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::subtraction, details::make_dynamic_action(_sub), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::multiplication, details::make_dynamic_action(_mul), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::division, details::make_dynamic_action(_div), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::modulus, details::make_dynamic_action(_mod), {Parameter{"lhs", }, Parameter{"rhs",}  })
        .add_member_function(sol::meta_function::power_of, details::make_dynamic_action(_pow), {Parameter{"base", }, Parameter{"exponent",}  })
        .add_member_function(sol::meta_function::unary_minus, details::make_dynamic_action(_unm), {Parameter{"value",}  });

#ifdef GRUNK_WITH_RECIPE

        //TODO: Can I nest RecipeCallerProxy in RecipeCaller, just like in C++?
        register_type<RecipeCaller::Proxy>("RecipeCallerProxy", g)
        .add_member_function("to_feature", &RecipeCaller::Proxy::to_feature);

        register_type<Recipe::SubRecipe>("SubRecipe", g)
        .set(
            sol::meta_function::call,
            [](Recipe::SubRecipe const& sr){ return sr(); }
        );

        register_type<RecipeCaller>("RecipeCaller", g)
        .set(
            sol::meta_function::index, 
            [](RecipeCaller& rc, std::string const& key) { 
                return rc.get(key); 
            }
        )
        .set(
            sol::meta_function::new_index, 
            [](RecipeCaller& rc, std::string const& key, sol::object const& value){ 
                if (value.is<DynamicFeature>()) {
                    return rc[key] = value.as<DynamicFeature>(); 
                } else {
                    return rc[key] = value;
                }
            }
        )
        .add_member_function("get", &RecipeCaller::get)
        .add_member_function("locked", &RecipeCaller::locked);

#endif

    }

    /**
     * @brief decorate_value applies the same decoration rule uniformly, however deep a
     * lookup chain has descended: a function_meta becomes a tracked action, a table
     * becomes a recursively decorated view of itself (see decorate_table), and anything
     * else is returned unchanged.
     *
     * This is what lets a type or function stay properly tracked no matter how many
     * namespace tables it sits behind (e.g. a plugin's own table, itself possibly
     * nested), rather than only directly inside original_env.
     */
    inline sol::object decorate_value(sol::object const& result)
    {
        if (result.is<function_meta>()) {
            return sol::make_object(lua, details::make_dynamic_action(result.as<function_meta>()));
        } else if (result.is<sol::table>()) {
            return sol::make_object(lua, decorate_table(result.as<sol::table>()));
        }
        return result;
    }

    /**
     * @brief decorate_table builds a decorated view of a table (a usertype, a module,
     * a plugin namespace, ...): a fresh table whose __index lazily decorates whatever
     * it finds in `source`, recursively, via decorate_value. A "new_feature" convenience
     * constructor is added if `source` itself looks constructible (has a "new" entry).
     *
     * Unlike the top-level decorated_env, this decorated view is not itself cached
     * anywhere - it is rebuilt each time its enclosing entry is looked up. That matches
     * this function's only prior (one-level-deep, non-recursive) use in
     * create_decorated_environment.
     */
    inline sol::table decorate_table(sol::table const& source)
    {
        sol::table decorated = lua.create_table();
        sol::table meta = lua.create_table();

        meta.set_function("__index", [this, source](sol::table, std::string const& key) -> sol::object {
            return decorate_value(source[key]);
        });
        decorated[sol::metatable_key] = meta;

        // intercept constructors to add a new_feature method
        if (source["new"].valid()) {
            sol::protected_function ctor = source["new"];
            decorated["new_feature"] = [ctor](sol::variadic_args args) -> DynamicFeature {
                sol::protected_function_result ret = ctor(args);
                if (!ret.valid()) {
                    sol::error err = ret;
                    throw std::runtime_error(std::string("Construction error: ") + err.what());
                }
                grunk::object obj = ret;
                return grunk::feature(obj);
            };
        }

        return decorated;
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
        mt.set_function("__index", [this](sol::table ts, std::string const& key) -> sol::object {

            sol::object result = original_env[key];  // Lookup in original environment

            if (!result.valid()) {
                return lua.globals()[key]; // use globals as fallback, but without decorating callables
            }

            decorated_env[key] = decorate_value(result);
            return decorated_env[key];
        });

        // Set the decorated metatable on the new environment
        decorated_env[sol::metatable_key] = mt;
    }

    /**
     * @brief create_module creates a fresh, named module table in the original environment.
     *
     * The table's metatable falls back to the original environment for lookups, so module
     * scripts can reference other registered types, functions and modules.
     */
    inline sol::table create_module(std::string const& name)
    {
        sol::table module = lua.create_table();
        sol::table mt = lua.create_table();
        mt["__index"] = original_env;
        module[sol::metatable_key] = mt;

        original_env.set(name, module);
        return module;
    }

    sol::state lua;
    sol::environment original_env;
    sol::environment decorated_env;
    std::vector<PluginInfo> m_plugins;
};

} // namespace grunk
