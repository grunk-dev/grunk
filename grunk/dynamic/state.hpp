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

#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <typeindex>
#include <unordered_map>

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
 * state::load_compiled_plugin), a Lua script (see state::load_lua_plugin_script/
 * state::load_lua_plugin_file), or plain C++ calling register_type/register_function
 * directly against the table returned by state::begin_plugin(info).
 *
 * @ingroup dynamic
 */
struct PluginInfo
{
    std::string name;
    std::string version;
};

// Forward declaration - state::begin_plugin/state::load_compiled_plugin return this (see
// plugin_namespace's own definition below, after `class state`: its methods call back into
// state's public register_*/modify_type API, so state must be a complete type by then).
class plugin_namespace;

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
        lua["grunk"]["plugins"] = lua.create_table();
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
     * @param qualifier optional dotted path under which `table` itself is reachable
     *                  (e.g. "myplugin" for a plugin namespace - see state::begin_plugin),
     *                  used to give the type's constructor/methods their fully-qualified
     *                  name. Left empty for `table` itself being reachable unqualified
     *                  (the default, flat-in-original_env case) - unless `table` is a
     *                  module created via create_module (e.g. begin_plugin's or
     *                  run_module_script's own table), in which case that module's own
     *                  name is used automatically (see module_qualifier). This is what
     *                  ActionDynamic::serialize embeds verbatim, and that text must
     *                  resolve correctly when a saved recipe is read back in - see
     *                  decorate_module_functions/register_external_type for the same
     *                  convention applied to the other plugin kinds.
     * @returns a usertype_proxy<T> to allow method chaining
     */
    template <typename T, sol::automagic_flags Flags = sol::automagic_flags::all>
    auto register_type(std::string const& name, std::optional<sol::table> table = std::nullopt, std::string const& qualifier = "")
    {
        if (!table) {
            table = original_env;
        }
        std::string const owner_module = module_qualifier(*table);
        std::string const qualified_name = qualify(name, *table, qualifier);
        auto proxy = usertype_proxy<T>{qualified_name, table->new_usertype<T>(name, sol::constant_automagic_enrollments<Flags>{})};
        // record T's usertype table under its C++ type, so a DynamicFeature carrying a
        // type hint for T (see DynamicFeature::type_hint) can later look up a method by
        // name without ever needing to evaluate its value - see the Feature usertype's
        // sol::meta_function::index handler in init() below. m_type_names mirrors this,
        // keyed the same way, purely so that handler can report a useful type name in
        // its error messages instead of just an opaque std::type_index.
        std::type_index const type = std::type_index(typeid(T));
        m_type_registry[type] = (*table)[name];
        m_type_names[type] = qualified_name;
        // Record which namespace (if any) this type belongs to, so forget_plugin can
        // remove it from the type registry if that namespace is later rolled back -
        // note this is keyed by `owner_module` (the table's own tag), not by
        // `qualifier`, since the latter may be an explicit override that doesn't
        // reflect which module table this type actually lives in.
        if (!owner_module.empty()) {
            m_module_types[owner_module].push_back(type);
        }
        return proxy;
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
     * @param qualifier optional dotted path under which `table` itself is reachable -
     *                  see register_type's own `qualifier` parameter for the full
     *                  explanation; this plays the same role here.
     * @returns a usertype_proxy<T> to allow method chaining (e.g. `.with_std_vector()`)
     */
    template <typename T>
    auto modify_type(std::string const& name, std::optional<sol::table> table = std::nullopt, std::string const& qualifier = "")
    {
        if (!table) {
            table = original_env;
        }
        // modify_type extends an *existing* usertype - unlike register_type, it has no
        // table[name] of its own to create, so a missing/wrong entry here (e.g. a typo,
        // or `table` not being where T was actually registered) must not be allowed to
        // silently overwrite the process-wide type registry with an invalid reference;
        // that would corrupt native colon-call dispatch for every other, unrelated
        // instance of T already relying on the earlier, correct registration.
        sol::object existing = (*table)[name];
        if (!existing.valid() || !existing.is<sol::table>()) {
            throw std::logic_error(
                "modify_type: no existing usertype named \"" + name + "\" found in the given "
                "table - modify_type extends an already-registered type, it does not create "
                "one (use register_type for that)."
            );
        }
        lua_State* L = lua;
        int table_idx = (*table).push(L);
        sol::usertype<T> ut(L, table_idx);
        lua_pop(L, 1);
        std::string const owner_module = module_qualifier(*table);
        std::string const qualified_name = qualify(name, *table, qualifier);
        // Populate the type registry directly here, the same way register_type does,
        // rather than relying on T having already been registered via register_type
        // against this same table/name (which happened to make this work before, since
        // both would resolve to the same underlying Lua table, but isn't guaranteed if
        // T's usertype was created some other way, e.g. by a plugin).
        std::type_index const type = std::type_index(typeid(T));
        m_type_registry[type] = existing.as<sol::table>();
        m_type_names[type] = qualified_name;
        // See register_type's identical bookkeeping for why this is keyed by
        // `owner_module` rather than `qualifier`.
        if (!owner_module.empty()) {
            m_module_types[owner_module].push_back(type);
        }
        return usertype_proxy<T>{qualified_name, ut};
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
     * @param qualifier optional dotted path under which `table` itself is reachable -
     *                  see register_type's own `qualifier` parameter for the full
     *                  explanation; this plays the same role here.
     */
    template <typename Func>
    void register_function(std::string const& name, Func&& fun, std::vector<Parameter> params = {}, std::optional<sol::table> table = std::nullopt, std::string const& qualifier = "")
    {
        // set function

        if (!table) {
            table = original_env;
        }
        std::string const qualified_name = qualify(name, *table, qualifier);
        auto meta_func = create_function_meta(lua, qualified_name, params, std::forward<Func>(fun));
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

        decorate_module_functions(module, name);
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
     * @param qualifier the dotted path under which `table` itself is reachable (e.g.
     *                  "adtl" for a plugin namespace, "mymod.sub" for a nested module
     *                  table), used to give every function_meta its fully-qualified
     *                  name. Left empty for `table` itself being reachable unqualified.
     *                  This is what ActionDynamic::serialize embeds verbatim, and that
     *                  text must resolve correctly when a saved recipe is read back in -
     *                  see register_external_type for the same convention.
     */
    inline void decorate_module_functions(sol::table table, std::string const& qualifier = "")
    {
        for (auto& kv : table) {
            sol::object key = kv.first;
            sol::object value = kv.second;

            if (!key.is<std::string>()) {
                continue;
            }

            std::string const function_name = key.as<std::string>();
            std::string const qualified_name = qualifier.empty() ? function_name : qualifier + "." + function_name;

            if (value.is<sol::table>()) {
                decorate_module_functions(value.as<sol::table>(), qualified_name);
            } else if (value.get_type() == sol::type::function) {
                sol::protected_function func = value.as<sol::protected_function>();
                table.set(function_name, create_function_meta(lua, qualified_name, {}, func));
            }
        }
    }

    /**
     * @brief begin_plugin starts a plugin whose types/functions are registered directly
     * from C++, by returning a plugin_namespace proxy for the caller to register
     * types/functions/external types against, and recording the plugin's identity in
     * plugins() immediately (there is no separate "finish" step - registration against
     * the returned proxy can happen incrementally, exactly like a module built via
     * run_module_script).
     *
     * This is the "plain C++" plugin kind's counterpart to load_compiled_plugin: instead
     * of loading a compiled Lua C extension, a plugin's own entry point (see
     * grunk::plugin's native loader) calls this once to obtain its namespace, then uses
     * the returned plugin_namespace's own register_type/register_function calls, which
     * already know their own namespace table and qualifier - no need to repeat either
     * one at every call:
     *
     * @code
     * extern "C" void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info) {
     *     auto ns = state.begin_plugin(info);
     *     ns.register_type<gp_Pnt>("gp_Pnt") ...;
     *     ns.register_function("bezier_curve", ...);
     * }
     * @endcode
     *
     * A plugin name already recorded in plugins() cannot be started again on the same
     * state - this throws io_error rather than silently discarding the first plugin's
     * namespace table (see create_module). Call clear_module(info.name) first if you
     * genuinely intend to reload a plugin under the same name.
     *
     * @param info the plugin's name and version. `info.name` doubles as the namespace
     *             table's key in the original environment, exactly like load_compiled_plugin.
     * @return a plugin_namespace proxy wrapping the plugin's (initially empty) namespace
     *         table - see plugin_namespace's own doc comment, defined below this class.
     */
    plugin_namespace begin_plugin(PluginInfo const& info);

    /**
     * @brief load_compiled_plugin loads a compiled Lua C extension (e.g. a SWIG-Lua
     * module) as a grunk plugin: its own table is registered as a namespace under its
     * name in the original environment, its free functions are made grunk-trackable,
     * and its identity is recorded in plugins().
     *
     * Classes the module exposes (e.g. adtl.adouble) are not yet usable as grunk types
     * after this call - their constructor/methods aren't plain table entries (see
     * external_type_proxy for why), so they still need bridging via the returned
     * plugin_namespace's own register_external_type (passing just the class's own short
     * name, e.g. "adouble" - it auto-prefixes the plugin's qualifier for you).
     *
     * A plugin name already recorded in plugins() cannot be loaded again on the same
     * state - this throws io_error rather than silently discarding the first plugin's
     * namespace table (see load_module). Call clear_module(info.name) first if you
     * genuinely intend to reload a plugin under the same name.
     *
     * @param info the plugin's name and version. `info.name` doubles as the module's
     *             loading key (see load_module) and thus must match the module's own
     *             internal identity (e.g. SWIG's `%module` name).
     * @param open_fn the module's C entry point (e.g. `luaopen_adtl`)
     * @return a plugin_namespace proxy wrapping the plugin's namespace table (the
     *         module's own table, decorated so its free functions are grunk-tracked) -
     *         see plugin_namespace's own doc comment, defined below this class. Index
     *         into it (`ns["SomeClass"]`) to reach entries the module itself defined,
     *         e.g. a class's constructor table to pass to register_external_type.
     */
    plugin_namespace load_compiled_plugin(PluginInfo const& info, lua_CFunction open_fn);

    /**
     * @brief load_lua_plugin_script starts a "pure Lua" plugin: no compilation, just a
     * name+version and some Lua source. Thin wrapper over run_module_script that
     * additionally records the plugin's identity in plugins(), exactly like
     * load_compiled_plugin/begin_plugin do for their own kinds - a plain
     * run_module_script call has no way to attach a version to what it creates, since
     * a module (unlike a plugin) has no identity of its own.
     *
     * Unlike load_compiled_plugin's `info.name`, which must match a compiled module's
     * own internal identity, a Lua-script plugin has no such constraint: its identity
     * is entirely up to the caller, since a bare .lua file carries no name/version of
     * its own (see grunk::plugin's native loader for the compiled-plugin case, where
     * that identity instead comes from the plugin's own `grunk_plugin_info` symbol).
     *
     * A plugin name already recorded in plugins() cannot be started again on the same
     * state - this throws io_error rather than silently augmenting the first plugin's
     * namespace table (see run_module_script). Call clear_module(info.name) first if
     * you genuinely intend to reload a plugin under the same name.
     *
     * @param info the plugin's name and version. `info.name` doubles as the module
     *             name run_module_script populates.
     * @param script the Lua source code to execute
     * @return the plugin's namespace table
     */
    inline sol::table load_lua_plugin_script(PluginInfo const& info, std::string const& script)
    {
        assert_plugin_name_free(info.name);
        try {
            run_module_script(info.name, script);
        } catch (...) {
            // run_module_script may have already created (and partially populated) the
            // module table before the script itself failed. assert_plugin_name_free
            // just confirmed info.name was previously free, so it's always safe to
            // fully vacate it here - otherwise a corrected retry under the same name
            // would hit assert_plugin_name_free's "already in use" error instead of
            // succeeding.
            forget_plugin(info.name);
            throw;
        }
        note_plugin(info);
        sol::table ns = original_env[info.name];
        return ns;
    }

    /**
     * @brief load_lua_plugin_file loads a Lua file from disk and starts a plugin from
     * it, following the same semantics as load_lua_plugin_script (see run_module_file's
     * relationship to run_module_script, which this mirrors).
     *
     * @param info the plugin's name and version
     * @param filename path to a Lua file
     * @return the plugin's namespace table
     */
    inline sol::table load_lua_plugin_file(PluginInfo const& info, std::string const& filename)
    {
        std::ifstream fin(filename);
        if (!fin) {
            throw io_error("Could not open plugin file: " + filename);
        }
        std::string content((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
        return load_lua_plugin_script(info, content);
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
     * @brief forget_plugin removes a plugin's identity from plugins() and the
     * lua["grunk"]["plugins"] table note_plugin populated, clears the namespace table
     * (if any) it occupied in the original/decorated environments, clears its entry (if
     * any) in Lua's own module cache (LUA_LOADED_TABLE - see load_module/
     * load_compiled_plugin), and removes any C++ types registered under its namespace
     * from the type registry (see register_type/modify_type) - so a forgotten plugin is
     * fully gone, not just missing from plugins().
     *
     * This is the low-level counterpart to note_plugin: grunk::plugin::load_native uses
     * it to roll back a plugin's recorded identity *and* whatever it managed to
     * register before throwing partway through (so a half-registered plugin is never
     * reported as loaded, and its partially-registered types/functions aren't left
     * reachable either - see load_native), load_lua_plugin_script uses it to roll back a
     * script that failed partway through, and clear_module uses it to fully vacate a
     * plugin's name before a reload.
     *
     * Does nothing if @p name is not currently recorded as a loaded plugin.
     *
     * @param name the plugin's name, as recorded in a prior note_plugin call
     */
    inline void forget_plugin(std::string const& name)
    {
        m_plugins.erase(
            std::remove_if(m_plugins.begin(), m_plugins.end(), [&](PluginInfo const& p) { return p.name == name; }),
            m_plugins.end()
        );
        sol::table plugins_table = lua["grunk"]["plugins"];
        plugins_table[name] = sol::lua_nil;

        original_env.set(name, sol::lua_nil);
        decorated_env.set(name, sol::lua_nil);

        // luaL_requiref (used by load_module/load_compiled_plugin) independently
        // caches modules by name in the registry's LUA_LOADED_TABLE, regardless of
        // original_env/decorated_env - without clearing that too, a subsequent
        // load_compiled_plugin call for the same name would see it already cached and
        // silently return the stale table instead of re-invoking open_fn.
        lua_State* L = lua;
        luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
        lua_pushnil(L);
        lua_setfield(L, -2, name.c_str());
        lua_pop(L, 1);

        // Remove any C++ types register_type/modify_type recorded as belonging to this
        // namespace, so a forgotten plugin's partial type registrations aren't left
        // reachable via the Feature usertype's native colon-call dispatch (see init()).
        if (auto it = m_module_types.find(name); it != m_module_types.end()) {
            for (std::type_index const& type : it->second) {
                m_type_registry.erase(type);
                m_type_names.erase(type);
            }
            m_module_types.erase(it);
        }
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
        // Unlike register_type/register_function, `name` here must already be fully
        // qualified (see this method's own doc comment) rather than auto-inferring a
        // qualifier from an implicit bare name - but a caller forgetting to prefix it
        // with `table`'s own namespace (e.g. passing "Bar" instead of "myplugin.Bar")
        // would otherwise silently register a type whose ctor/methods serialize with
        // an unresolvable, unqualified name. Catch that loudly instead.
        if (std::string const qualifier = module_qualifier(*table); !qualifier.empty()) {
            std::string const prefix = qualifier + ".";
            if (name.compare(0, prefix.size(), prefix) != 0) {
                throw std::logic_error(
                    "register_external_type: \"" + name + "\" was registered against a "
                    "namespace table qualified as \"" + qualifier + "\", but does not start "
                    "with \"" + prefix + "\" - its constructor/methods would serialize with "
                    "an unresolvable name. Pass \"" + prefix + name + "\" instead."
                );
            }
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
     * incremental-augmentation behavior. This is also the sanctioned way to reload a
     * plugin under the same name: begin_plugin/load_compiled_plugin/load_lua_plugin_script
     * all refuse to start a plugin whose name is already recorded in plugins() (see
     * assert_plugin_name_free), so clear_module(name) - which also forgets the name via
     * forget_plugin - must be called first.
     *
     * @param name name of the module (or plugin) to remove
     */
    inline void clear_module(std::string const& name)
    {
        // forget_plugin clears original_env/decorated_env[name], the LUA_LOADED_TABLE
        // cache entry, and any registered types recorded under this name - so a plain
        // (non-plugin) module is cleared exactly the same way a plugin is.
        forget_plugin(name);
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
        DynamicFeature f = grunk::feature(object(sol::make_object(lua, value)));
        f.set_type_hint(std::type_index(typeid(T)));
        return f;
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
        DynamicFeature f = feature(obj);
        if (auto hint = ctor_return_type_hint(usertype)) {
            f.set_type_hint(*hint);
        }
        return f;
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

        // Backs DynamicFeature::call()'s relative-name (not fully-qualified) overload:
        // self[method](self, ...) is exactly what Lua's own `self:method(...)` desugars
        // to, so delegating to it here reuses the Feature usertype's real index
        // resolution (built-in members first, then the sol::meta_function::index
        // fallback below, keyed off the type hint) instead of duplicating it in C++.
        // Compiled once here rather than per-call.
        lua.script("function grunk.__member_call(self, method, ...) return self[method](self, ...) end");

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
        .add_member_function(sol::meta_function::unary_minus, details::make_dynamic_action(_unm), {Parameter{"value",}  })
        .set(sol::meta_function::index, [this](DynamicFeature const& self, std::string const& key) -> sol::object {
            // Native colon-call dispatch fallback: sol2 checks Feature's own members
            // (value, set_value, id, ..., as) before this ever runs, so those always win
            // over a same-named method on the wrapped type. If self carries a static
            // type hint (see DynamicFeature::type_hint - populated at construction time,
            // never by evaluating self), look up "key" directly on that type's usertype
            // table and return a tracked/decorated closure for it - exactly like the
            // qualified TypeName.method(instance, ...) form, just reached via self:key(...)
            // instead. Never falls back to evaluating self just to answer this query.
            //
            // The three failure modes below are distinguished because they call for
            // different fixes: no hint at all means the feature's origin is untracked
            // (grunk.feature(rawObject) et al.) and needs an explicit :as(Type)/qualified
            // call; a hint whose type was never registered is an internal inconsistency
            // (type_hint() and m_type_registry/m_type_names are always populated
            // together, see register_type/modify_type); a known type missing the member
            // is very likely just a typo'd method name.
            auto hint = self.type_hint();
            if (!hint) {
                throw std::runtime_error(
                    "Feature: no static type information available for \"" + key +
                    "\" - use :as(Type) explicitly, or ensure this feature comes from a "
                    "registered constructor/member function call."
                );
            }

            auto name_it = m_type_names.find(*hint);
            auto registry_it = m_type_registry.find(*hint);
            if (registry_it == m_type_registry.end() || name_it == m_type_names.end()) {
                throw std::runtime_error(
                    "Feature: this feature's type hint has no corresponding usertype "
                    "registered in this state - cannot resolve \"" + key + "\". Use "
                    ":as(Type) explicitly instead."
                );
            }

            sol::object found = registry_it->second[key];
            if (!found.valid()) {
                throw std::runtime_error(
                    "Feature: type \"" + name_it->second + "\" has no member \"" + key +
                    "\" - use :as(Type) explicitly if this is intentional."
                );
            }
            return decorate_value(found);
        });

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
    /**
     * @brief looks up the return type hint stored on a usertype's constructor
     * function_meta (see usertype_proxy::add_constructors, which always stamps this
     * with the usertype's own C++ type - a constructor's result is always exactly T).
     * Used to give literal features built by directly invoking a constructor (as
     * opposed to going through ActionDynamic, which stamps hints itself) the same type
     * hint an action-based construction would get - see feature(sol::table, Args...)
     * and decorate_table's new_feature below.
     */
    static std::optional<std::type_index> ctor_return_type_hint(sol::table const& usertype)
    {
        sol::object new_obj = usertype["new"];
        if (new_obj.is<function_meta>()) {
            return new_obj.as<function_meta>().return_type_hint();
        }
        return std::nullopt;
    }

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
            auto hint = ctor_return_type_hint(source);
            decorated["new_feature"] = [ctor, hint](sol::variadic_args args) -> DynamicFeature {
                sol::protected_function_result ret = ctor(args);
                if (!ret.valid()) {
                    sol::error err = ret;
                    throw std::runtime_error(std::string("Construction error: ") + err.what());
                }
                grunk::object obj = ret;
                DynamicFeature f = grunk::feature(obj);
                if (hint) {
                    f.set_type_hint(*hint);
                }
                return f;
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
     * @brief note_plugin records a plugin's identity: it is appended to plugins() and
     * mirrored into the Lua-global "grunk.plugins" table (alongside env/parametric_env),
     * so anything that only has access to this state's lua_State - like grunk::Recipe,
     * which has no back-reference to the state that created it - can still discover
     * which plugins are loaded, e.g. to populate/validate a recipe's "uses" block.
     *
     * Every load_*_plugin method (whatever mechanism it uses to populate the plugin's
     * own namespace table) calls this once it has done so, giving all plugin kinds the
     * same discoverable identity.
     */
    inline void note_plugin(PluginInfo const& info)
    {
        m_plugins.push_back(info);
        sol::table plugins_table = lua["grunk"]["plugins"];
        plugins_table[info.name] = info.version;
    }

    /**
     * @brief create_module creates a fresh, named module table in the original environment.
     *
     * The table's metatable falls back to the original environment for lookups, so module
     * scripts can reference other registered types, functions and modules. The metatable
     * also records the module's own name (see module_qualifier), so register_type/
     * register_function calls against this table can auto-infer their `qualifier`
     * argument instead of requiring every call to repeat it.
     */
    inline sol::table create_module(std::string const& name)
    {
        sol::table module = lua.create_table();
        sol::table mt = lua.create_table();
        mt["__index"] = original_env;
        module[sol::metatable_key] = mt;
        tag_module_name(module, name);

        original_env.set(name, module);
        return module;
    }

    /**
     * @brief tag_module_name records @p name on @p table's metatable (creating one if
     * @p table doesn't already have one) as the dotted path @p table itself is
     * reachable under - see module_qualifier, which reads this back.
     *
     * create_module uses this for a freshly-created table; load_compiled_plugin uses
     * it directly on the pre-existing table load_module/luaL_requiref returns (which
     * has no create_module-style metatable of its own), so both plugin kinds let
     * register_type/register_function/register_external_type auto-infer their
     * qualifier the same way.
     */
    inline void tag_module_name(sol::table& table, std::string const& name)
    {
        sol::object existing_mt = table[sol::metatable_key];
        sol::table mt = (existing_mt.valid() && existing_mt.is<sol::table>())
            ? existing_mt.as<sol::table>()
            : lua.create_table();
        mt["__grunk_module_name"] = name;
        table[sol::metatable_key] = mt;
    }

    /**
     * @brief module_qualifier returns the dotted path @p table itself is reachable
     * under, if @p table was created via create_module (e.g. begin_plugin's or
     * run_module_script's own namespace table) - otherwise "".
     *
     * register_type/register_function/modify_type use this to auto-infer their own
     * `qualifier` argument when the caller leaves it empty, so a plugin's
     * grunk_plugin_register (or any code registering against a module table) doesn't
     * have to pass e.g. info.name to every single registration call just to keep
     * constructors/methods/functions serializing with a resolvable, fully-qualified
     * name (see register_type's `qualifier` parameter). Registering directly against
     * original_env (the default, unqualified case) correctly yields "" here too, since
     * original_env was never created via create_module.
     */
    inline std::string module_qualifier(sol::table const& table) const
    {
        sol::object mt = table[sol::metatable_key];
        if (mt.valid() && mt.is<sol::table>()) {
            sol::object tag = mt.as<sol::table>()["__grunk_module_name"];
            if (tag.valid() && tag.is<std::string>()) {
                return tag.as<std::string>();
            }
        }
        return "";
    }

    /**
     * @brief qualify computes the fully-qualified, dotted name a registered
     * type/function's constructor/methods should serialize with - see register_type's
     * `qualifier` parameter for the full explanation of the rule this implements.
     *
     * Shared by register_type, modify_type and register_function, which otherwise each
     * repeated this exact computation.
     *
     * @param name the type/function's own (unqualified) name
     * @param table the namespace table it is being registered into
     * @param qualifier an explicit override for `table`'s own qualifier, or "" to
     *                  auto-infer it via module_qualifier
     */
    inline std::string qualify(std::string const& name, sol::table const& table, std::string const& qualifier) const
    {
        std::string const effective_qualifier = !qualifier.empty() ? qualifier : module_qualifier(table);
        return effective_qualifier.empty() ? name : effective_qualifier + "." + name;
    }

    /**
     * @brief assert_plugin_name_free throws io_error if a plugin named @p name is
     * already recorded in plugins() on this state, or if @p name is already occupied
     * by something else entirely (e.g. a plain module created via run_module_script).
     *
     * begin_plugin/load_compiled_plugin/load_lua_plugin_script all call this before
     * touching any table, so loading a plugin a second time under the same name fails
     * loudly instead of silently discarding (begin_plugin/load_compiled_plugin) or
     * silently augmenting (load_lua_plugin_script) the first plugin's namespace table -
     * and starting a plugin under a name already taken by a plain module fails loudly
     * instead of begin_plugin/load_compiled_plugin silently overwriting that module's
     * table. Call clear_module(name) first to intentionally reuse a name.
     */
    inline void assert_plugin_name_free(std::string const& name) const
    {
        for (PluginInfo const& p : m_plugins) {
            if (p.name == name) {
                throw io_error(
                    "A plugin named \"" + name + "\" (version " + p.version + ") is already "
                    "loaded in this grunk::state. Call clear_module(\"" + name + "\") first if "
                    "you intend to reload it."
                );
            }
        }
        if (sol::object existing = original_env[name]; existing.valid()) {
            throw io_error(
                "\"" + name + "\" is already in use in this grunk::state (e.g. a module "
                "created via run_module_script), not as a plugin. Call clear_module(\"" + name +
                "\") first if you intend to reuse this name for a plugin."
            );
        }
    }

    sol::state lua;
    sol::environment original_env;
    sol::environment decorated_env;
    std::vector<PluginInfo> m_plugins;

    /// @brief maps a registered C++ type to its usertype table, keyed by std::type_index -
    /// see register_type and the Feature usertype's sol::meta_function::index handler.
    std::unordered_map<std::type_index, sol::table> m_type_registry;

    /// @brief maps a registered C++ type to its display name, keyed the same way as
    /// m_type_registry - only used to phrase useful error messages in the Feature
    /// usertype's sol::meta_function::index handler.
    std::unordered_map<std::type_index, std::string> m_type_names;

    /// @brief maps a module/plugin name (see module_qualifier) to the C++ types
    /// register_type/modify_type registered against tables tagged with that name - see
    /// forget_plugin, which uses this to remove a rolled-back plugin's types from
    /// m_type_registry/m_type_names too.
    std::unordered_map<std::string, std::vector<std::type_index>> m_module_types;
};

/**
 * @brief plugin_namespace is a proxy returned by state::begin_plugin/
 * state::load_compiled_plugin: it remembers the namespace table and qualifier a plugin
 * registers into, so register_type/register_function/register_external_type calls
 * against it don't need to repeat either one - `ns.register_type<T>("T")` instead of
 * `state.register_type<T>("T", ns, info.name)`.
 *
 * For register_type/register_function/modify_type this is purely convenience: their
 * `qualifier` argument is already auto-inferred from the table when left empty (see
 * state::qualify/module_qualifier), so passing `info.name` explicitly was always
 * redundant, just no longer necessary to spell out at every call. register_external_type
 * is different: unlike the others, it does not auto-construct a qualified name from its
 * `table` argument, it only validates that the caller-supplied name already starts with
 * the table's qualifier (throwing std::logic_error otherwise) - so
 * plugin_namespace::register_external_type actually does new work, prefixing a short
 * name (e.g. "MyClass") into the fully-qualified one (state::register_external_type
 * itself still requires) automatically.
 *
 * Normally obtained via state::begin_plugin or state::load_compiled_plugin, not
 * constructed directly. Implicitly convertible to sol::table (and table()/operator[]
 * expose the wrapped table directly) as an escape hatch for code that needs the raw
 * table - e.g. to pass into an API taking a plain sol::table, or to call one of state's
 * own register_type/register_function/modify_type methods directly with an explicit
 * qualifier override.
 *
 * @ingroup dynamic
 */
class plugin_namespace
{
public:
    plugin_namespace(state& s, sol::table ns, std::string qualifier)
     : m_state(s)
     , m_ns(std::move(ns))
     , m_qualifier(std::move(qualifier))
    {}

    /// @brief see state::register_type - `name`/`table`/`qualifier` are this
    /// namespace's own name/table/qualifier, so only the type's own name is needed here.
    template <typename T, sol::automagic_flags Flags = sol::automagic_flags::all>
    auto register_type(std::string const& name)
    {
        return m_state.register_type<T, Flags>(name, m_ns, m_qualifier);
    }

    /// @brief see state::modify_type.
    template <typename T>
    auto modify_type(std::string const& name)
    {
        return m_state.modify_type<T>(name, m_ns, m_qualifier);
    }

    /// @brief see state::register_function.
    template <typename Func>
    void register_function(std::string const& name, Func&& fun, std::vector<Parameter> params = {})
    {
        m_state.register_function(name, std::forward<Func>(fun), std::move(params), m_ns, m_qualifier);
    }

    /// @brief see state::register_external_type. Unlike that method, `name` here is
    /// just the type's own short name (e.g. "MyClass", not "my_plugin.MyClass") - this
    /// auto-prefixes this namespace's own qualifier onto it, rather than requiring the
    /// caller to spell out the fully-qualified name by hand.
    external_type_proxy register_external_type(std::string const& name, sol::protected_function ctor, sol::object probe = sol::lua_nil)
    {
        std::string const qualified = m_qualifier.empty() ? name : m_qualifier + "." + name;
        return m_state.register_external_type(qualified, ctor, m_ns, probe);
    }

    /// @brief looks up an entry already present in this namespace's table - e.g. a
    /// compiled-Lua module's own class constructor table, to pass to
    /// register_external_type.
    sol::object operator[](std::string const& key) const
    {
        return m_ns[key];
    }

    /// @brief the wrapped namespace table - see this class's own doc comment for when
    /// you'd need this instead of this class's own register_*/modify_type methods.
    sol::table const& table() const
    {
        return m_ns;
    }

    /// @brief the dotted path this namespace's own table is reachable under (e.g. the
    /// owning plugin's name) - see state::module_qualifier.
    std::string const& qualifier() const
    {
        return m_qualifier;
    }

    /// @brief escape hatch - see this class's own doc comment.
    operator sol::table() const
    {
        return m_ns;
    }

private:
    state& m_state;
    sol::table m_ns;
    std::string m_qualifier;
};

inline plugin_namespace state::begin_plugin(PluginInfo const& info)
{
    assert_plugin_name_free(info.name);
    sol::table ns = create_module(info.name);
    note_plugin(info);
    return plugin_namespace(*this, ns, info.name);
}

inline plugin_namespace state::load_compiled_plugin(PluginInfo const& info, lua_CFunction open_fn)
{
    assert_plugin_name_free(info.name);
    sol::table ns = load_module(info.name, open_fn);
    decorate_module_functions(ns, info.name);
    original_env.set(info.name, ns);
    // Tag ns with its own module name, exactly like create_module does for
    // begin_plugin's namespace table, so register_type/register_function/
    // register_external_type calls against it can auto-infer their qualifier via
    // module_qualifier instead of requiring info.name to be repeated at every call.
    tag_module_name(ns, info.name);
    note_plugin(info);
    return plugin_namespace(*this, ns, info.name);
}

} // namespace grunk
