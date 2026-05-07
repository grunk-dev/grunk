// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/core/Feature.hpp"
#include <object.hpp>
#include "grunk/dynamic/sol_helpers.hpp"

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
     , lua(nullptr) //TODO: This might be a problem. But we can't extract the lua state from the object without evaluating
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
            if (lua == nullptr) {
                // need to evalatue
                lua = value().lua_state();
            }
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
     * @brief returns the lua state of the feature. This is needed for creating new features from the value of this feature, e.g. when calling methods on the feature from Lua.
     * 
     * @return lua_State* the lua state of the feature
     */
    lua_State* lua_state() const {
        return lua;
    }

private:

    /** @brief checks if the lua state of the feature is initialized. This is needed for creating new features from the value of this feature, e.g. when calling methods on the feature from Lua. If the lua state is not initialized, an exception is thrown.
     * 
     * @throws std::runtime_error if the lua state is not initialized
     */
    void check_lua() const {
        if (!lua) {
            throw std::runtime_error("DynamicFeature: lua state is uninitialized");
        }
    }

    lua_State* lua;
};

using DynamicFeature = Feature<object>;


} // namespace grunk
