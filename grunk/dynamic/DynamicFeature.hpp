// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/core/Feature.hpp"
#include <object.hpp>
#include "grunk/dynamic/sol_helpers.hpp"

#include <optional>
#include <typeindex>
#include <utility>

namespace grunk {

/**
 * @brief Specialization of the Feature class for dynamic features.
 */
template <>
class Feature<object> : public FeatureBase<Feature<object>, object>
{
    using Base = FeatureBase<Feature<object>, object>;

public:

    /**
     * @brief constructs a dynamic feature with the given value. The value can be a literal, a variable or the output of an action. 
     *
     * @param v the value of the feature
     */
    Feature(object const& v)
     : Base(v)
     , lua(v.lua_state())
    {}

    /**
     * @brief constructs a dynamic feature from a parametric::param. This is used internally for cloning and other operations that manipulate the underlying DAG.
     *
     * @param p the parametric::param to construct the feature from
     */
    explicit Feature(parametric::param<object> const& p)
     : FeatureBase<Feature<object>,object>(p)
     , lua(nullptr) // action-derived feature: the lua state isn't known until evaluated
                    // (see check_lua, which self-heals this the same way set_value does)
    {}


    /**
     * @brief constructs an empty dynamic feature.
     * 
     */
    Feature(lua_State* lua_state = nullptr)
     : FeatureBase<Feature<object>, object>()
     , lua(lua_state)
    {}

    /**
     * @brief sets the value of the feature. If the value is an object, it is set directly. Otherwise, it is converted to an object using sol::make_object and the lua state of the feature.
     * 
     * @tparam T the type of the value to set
     * @param t the value to set
     */
    template <typename T>
    void set_value(T const& t) {
        if constexpr (std::is_same_v<T, object>) {
            this->Base::set_value(t);
        } else {
            check_lua(); // self-heals lua from value() if not yet known - see check_lua
            this->Base::set_value(sol::make_object(lua, t));
        }
    }

    /**
     * @brief Returns a sol::table that can be used as a usertype in Lua. The table has a __index metamethod that looks up the method in the given usertype table and returns a lambda function that calls the method with the feature as the first argument and the variadic arguments passed to the lambda as the remaining arguments.
     * 
     * @param usertype The sol::table representing the usertype to look up methods in
     * @return sol::table A sol::table that can be used as a usertype in Lua
     */
    sol::table as(sol::table usertype) const
    {
        check_lua();
        sol::state_view l(lua);
        sol::table method_table = l.create_table();
        sol::table mt = l.create_table();
        mt.set_function("__index", [this, usertype](sol::table, std::string const& method) -> sol::object {
            sol::protected_function func = usertype[method];
            return sol::make_object(lua, sol::as_function(
                [this, func](sol::variadic_args va){
                    return func(*this, va);
                }
            ));
        });
        method_table[sol::metatable_key] = mt;
        return method_table;
    }

    /**
     * @brief Returns a sol::table that can be used as a usertype in Lua. The table has a __index metamethod that looks up the method in the given usertype table and returns a lambda function that calls the method with the feature as the first argument and the variadic arguments passed to the lambda as the remaining arguments.
     * 
     * @param usertype The string identifier of the usertype to look up in the grunk state
     * @return sol::table A sol::table that can be used as a usertype in Lua
     */
    sol::table as(std::string const& usertype) const
    {
        check_lua();
        sol::state_view l(lua);
        sol::table usertype_table = details::lookup_nested(l["grunk"]["parametric_env"], usertype);
        return as(usertype_table);
    }

    /**
     * @brief Calls a member function of this feature's wrapped type by name, from C++ -
     * the C++-side equivalent of Lua's colon-call syntax (self:method(args)).
     *
     * Accepts two kinds of names:
     * - relative to the usertype (e.g. "pow"): resolved exactly like a Lua colon-call
     *   would be - Feature's own built-in members (value, set_value, id, ...) always win
     *   first; otherwise the wrapped type's registered method is looked up via this
     *   feature's type hint (see type_hint()) - never by evaluating this feature's
     *   value. Throws if no type hint is available, same as the Lua-level dispatch (see
     *   the sol::meta_function::index handler on the Feature usertype, state.hpp).
     * - fully qualified (e.g. "MyScalar.pow" or "MyScalar:pow" - the separator is purely
     *   cosmetic): resolved directly against the decorated environment and invoked with
     *   this feature passed as the leading argument, equivalent to writing
     *   MyScalar.pow(x, args...) in a recipe script. Works regardless of type hints.
     *
     * @param name the method name, fully-qualified or relative to the wrapped usertype
     * @param args the arguments to pass to the method
     * @return object the result of the call - typically a DynamicFeature (an
     *         uncomputed, dependency-tracked action), except when a reserved Feature
     *         member (like "value") wins, in which case it is that member's own result.
     */
    template <typename... Args>
    object call(std::string const& name, Args&&... args) const
    {
        check_lua();
        sol::state_view l(lua);

        auto unwrap = [&name](sol::protected_function_result ret) -> object {
            if (!ret.valid()) {
                sol::error err = ret;
                throw std::runtime_error("DynamicFeature::call(\"" + name + "\"): " + err.what());
            }
            return ret;
        };

        if (name.find('.') != std::string::npos || name.find(':') != std::string::npos) {
            sol::protected_function func = details::lookup_nested(l["grunk"]["parametric_env"], name);
            return unwrap(func(*this, std::forward<Args>(args)...));
        }

        sol::protected_function func = l["grunk"]["__member_call"];
        return unwrap(func(*this, name, std::forward<Args>(args)...));
    }

    /**
     * @brief returns the lua state of the feature. This is needed for creating new features from the value of this feature, e.g. when calling methods on the feature from Lua.
     *
     * @return lua_State* the lua state of the feature
     */
    lua_State* lua_state() const {
        return lua;
    }

    /**
     * @brief Records the C++ type this feature's (possibly not-yet-evaluated) value is
     * known to end up wrapping - populated at construction time from wherever that
     * type is statically known (a registered constructor/member function's return
     * type, see function_meta::return_type_hint and ActionDynamic::initialize_results;
     * or a literal feature's own concrete type, see state::feature<T>), *not* by
     * inspecting the value itself. This is what lets Feature's native colon-call
     * dispatch (state.hpp's Feature usertype registration) look up a method without
     * ever forcing evaluation just to answer a type query.
     *
     * @param type the wrapped value's C++ type, typically from typeid(T)
     */
    void set_type_hint(std::type_index type) {
        m_type_hint = type;
    }

    /**
     * @brief The type hint set via set_type_hint, if any. std::nullopt if this
     * feature's value's type was never statically known at construction time (e.g. a
     * feature built directly from a fully generic grunk::object/sol::object).
     */
    std::optional<std::type_index> type_hint() const {
        return m_type_hint;
    }

    /**
     * @brief clones this feature and its underlying DAG node (see FeatureBase::clone),
     * additionally carrying its type hint over to the clone - cloning never changes
     * the wrapped value's type, so this is a direct copy, not a re-derivation, and
     * never forces evaluation either.
     *
     * @param cloned_nodes see FeatureBase::clone
     * @return Feature a clone of this feature, with the same type hint
     */
    Feature clone(
        std::shared_ptr<parametric::DAGNode::ClonedNodeMap> cloned_nodes = parametric::DAGNode::new_cloned_node_map()
    ) const
    {
        Feature result = Base::clone(cloned_nodes);
        result.m_type_hint = m_type_hint;
        return result;
    }

private:

    /** @brief checks if the lua state of the feature is initialized, self-healing it first if not.
     *
     * A feature constructed from a parametric::param (any action-derived feature - the result
     * of a computation, not a literal/grunk.feature root) doesn't know its lua state up front
     * (see that constructor). Rather than fail outright, this forces evaluation - the same
     * self-healing set_value already does - and takes the lua state from the resulting value.
     * Only an entirely value-less feature (the default constructor, with no lua_state passed
     * either) can still fail here.
     *
     * @throws std::runtime_error if the lua state is not initialized and can't be recovered
     */
    void check_lua() const {
        if (!lua) {
            lua = value().lua_state();
        }
        if (!lua) {
            throw std::runtime_error("DynamicFeature: lua state is uninitialized");
        }
    }

    mutable lua_State* lua;

    /// @brief see set_type_hint/type_hint
    std::optional<std::type_index> m_type_hint;
};

using DynamicFeature = Feature<object>;


} // namespace grunk
