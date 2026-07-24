// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <sol/sol.hpp>
#include "grunk/dynamic/function_meta.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace grunk {

/**
 * @brief external_type_proxy registers a type that is not a compile-time C++ type
 * known to sol2 (as usertype_proxy<T> requires), but an opaque Lua value obtained
 * from elsewhere at runtime, e.g. a class exposed by a SWIG-Lua module loaded via
 * state::load_module.
 *
 * Such bindings typically expose a class as a "static" table whose metatable's
 * __call constructs new instances, with instance methods reachable only through
 * a separate, per-instance metatable (not through the static table itself). Neither
 * decorate_module_functions' plain pairs() traversal nor state::register_type's
 * sol::usertype<T> machinery can see those methods, so this proxy bridges them
 * generically instead: it constructs one probe instance of the type (by calling
 * the constructor with no arguments, unless a probe is supplied explicitly) and
 * looks methods up on the probe.
 *
 * The looked-up method reference is unbound (it expects the instance as its first
 * argument, like any Lua "method"), so the same reference obtained from the probe
 * is valid to call on any other instance of the type - the probe is only a vehicle
 * for discovering the method, not a receiver bound into the returned function.
 *
 * Methods do not need to be declared up front: the constructor installs a __index
 * metamethod on the type table that probes for any not-yet-known key the first time
 * it is looked up, and caches the result as a plain table entry if the probe resolves
 * it to a callable. SWIG-Lua's generated bindings expose no way to enumerate a
 * class's method names from Lua, only to look one up once you already know it (see
 * add_member_function) - this metamethod is what turns that per-name lookup into
 * "any method just works", so bridging a class costs one register_external_type call
 * regardless of how many methods it has. add_member_function/add_member_functions
 * remain available to attach Parameter metadata, restrict to an explicit allowlist,
 * or bridge a method whose name a probe can't discover on its own (e.g. because
 * ctor() with no arguments doesn't produce a valid instance and no probe was given).
 *
 * Each bridged member becomes a function_meta stored directly on a plain
 * sol::table (the "type" table, following the shape register_type produces),
 * so it participates in state::create_decorated_environment without any changes
 * there: dependency tracking and lazy evaluation work exactly as they do for a
 * usertype_proxy<T>-registered type.
 *
 * @ingroup dynamic
 */
class external_type_proxy {
public:

    /**
     * @brief constructs an external_type_proxy and registers the constructor.
     *
     * @param name_ the name of the type, used for function metadata and error messages
     * @param L_ the Lua state the type lives in
     * @param ctor_ the type's constructor, callable as ctor_(args...) to create an instance
     * @param table_ the table the type is registered into, e.g. original_env
     * @param probe_ an optional pre-built instance of the type, used to discover methods.
     *               If not given, a probe is lazily constructed by calling ctor_ with no
     *               arguments the first time a method is looked up (explicitly or via
     *               auto-discovery).
     */
    external_type_proxy(
        std::string const& name_,
        lua_State* L_,
        sol::protected_function ctor_,
        sol::table table_,
        sol::object probe_ = sol::lua_nil)
     : name(name_)
     , L(L_)
     , ctor(ctor_)
     , table(table_)
     , scratch(sol::state_view(L_).create_table())
    {
        table.set("new", create_function_meta(L, name + ".new", ctor));
        if (probe_.valid()) {
            scratch["probe"] = probe_;
        }

        // Auto-discovery fallback. This only ever runs for a key `table` doesn't
        // already raw-contain (Lua only consults __index on a miss), so it can never
        // shadow "new" above or anything add_member_function bridges explicitly,
        // whichever runs first.
        //
        // Every capture below is a value copy - sol::table/sol::protected_function are
        // cheap, refcounted handles onto the same underlying Lua objects, so copying
        // keeps them alive for the closure's lifetime and writes through the copy stay
        // visible through every other handle to the same table (notably `scratch` and
        // `table` below, shared with this proxy's own members of the same name).
        // Deliberately not `this`: external_type_proxy is a chain-returned temporary
        // that is gone long before this metamethod is ever invoked.
        sol::table meta = sol::state_view(L_).create_table();
        meta.set_function(
            "__index",
            [type_name = name_, L_, ctor_, target = table_, scratch = scratch](sol::table, std::string const& key) mutable -> sol::object {
                if (key.size() >= 2 && key[0] == '_' && key[1] == '_') {
                    // Metamethod-shaped names (__mul, __tostring, ...) are never
                    // ordinary methods; skip probing for them defensively, even
                    // though SWIG-Lua's own method dispatch already keeps them out
                    // of reach of a plain instance[key] lookup.
                    return sol::lua_nil;
                }

                sol::object probe = scratch["probe"];
                if (!probe.valid()) {
                    sol::protected_function_result res = ctor_();
                    if (!res.valid()) {
                        // No default constructor to probe with and none was supplied
                        // up front - this key simply can't be auto-discovered. Let it
                        // resolve to nil like any other missing key; the caller can
                        // still bridge it via add_member_function with an explicit
                        // probe instance.
                        return sol::lua_nil;
                    }
                    probe = sol::object(res);
                    scratch["probe"] = probe;
                }

                sol::object method = scratch["probe"][key];
                if (!method.valid() || method.get_type() != sol::type::function) {
                    return sol::lua_nil;
                }

                sol::protected_function pf = method;
                sol::object bridged = create_function_meta(L_, type_name + "." + key, {}, pf);
                target[key] = bridged;
                return bridged;
            }
        );
        table[sol::metatable_key] = meta;
    }

    /**
     * @brief bridges a single member function, found by looking it up on a probe
     * instance of the type.
     *
     * @param method_name the name of the method, as it would be called in Lua (e.g.
     *                    "obj:method_name(...)")
     * @param params optional metadata for the function parameters
     * @return a reference to this proxy for chaining
     */
    external_type_proxy& add_member_function(std::string const& method_name, std::vector<Parameter> params = {})
    {
        ensure_probe();

        sol::object method = scratch["probe"][method_name];
        if (!method.valid() || method.get_type() != sol::type::function) {
            throw std::runtime_error(
                "external_type_proxy \"" + name + "\": no method \"" + method_name
                + "\" found on a probe instance."
            );
        }

        sol::protected_function pf = method;
        table.set(method_name, create_function_meta(L, name + "." + method_name, params, pf));
        return *this;
    }

    /**
     * @brief bridges a list of member functions in one call. Equivalent to calling
     * add_member_function once per name, without per-method parameter metadata.
     *
     * @param method_names the names of the methods to bridge
     * @return a reference to this proxy for chaining
     */
    external_type_proxy& add_member_functions(std::vector<std::string> const& method_names)
    {
        for (auto const& method_name : method_names) {
            add_member_function(method_name);
        }
        return *this;
    }

private:

    void ensure_probe()
    {
        sol::object existing = scratch["probe"];
        if (existing.valid()) {
            return;
        }

        sol::protected_function_result res = ctor();
        if (!res.valid()) {
            sol::error err = res;
            throw std::runtime_error(
                "external_type_proxy \"" + name + "\": could not construct a probe instance "
                "to discover methods (" + err.what() + "). Construct one manually and pass it "
                "to register_external_type instead."
            );
        }
        sol::object probe = res;
        scratch["probe"] = probe;
    }

    std::string name;
    lua_State* L;
    sol::protected_function ctor;
    sol::table table;

    /// @brief scratch table used to store/look up the probe instance generically,
    /// regardless of its underlying Lua type (userdata, table, ...) - sol::object
    /// itself does not support indexing.
    sol::table scratch;
};

} // namespace grunk
