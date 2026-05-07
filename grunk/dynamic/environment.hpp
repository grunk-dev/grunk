// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/dynamic/DynamicFeature.hpp"
#include <sol/sol.hpp>

namespace grunk {

/**
 * @brief The environment class is a wrapper around a sol::environment, which represents a LUA environment. It provides methods to access variables stored in the environment and to evaluate LUA scripts in the environment. It also provides a method to tag features stored in the environment with their LUA variable name, which is useful for serialization.
 * 
 * @ingroup dynamic
 */
class environment 
{
public:

    /**
     * @brief constructs an environment from a sol::environment. This is used internally for creating environments from the state and for creating recipes from environments.
     * 
     * @param env the sol::environment to construct the environment from
     */
    environment(sol::environment const& env) : m_environment(env) {}

    /**
     * @brief returns a variable stored in the active environment
     * @param key The name of the variable
     * @return The queried variable
     */
    inline decltype(auto) operator[](std::string const& key)
    {
        return m_environment[key];
    }

    /**
     * @brief returns a variable stored in the active environment
     * @param key The name of the variable
     * @return The queried variable
     */
    inline decltype(auto) operator[](std::string const& key) const
    {
        return m_environment[key];
    }

    /**
     * @brief get returns a variable stored in the active environment and checks if it is valid. If the variable is not found in the environment, an exception is thrown.
     * 
     * @tparam T the type to cast the variable to. If T is sol::object, no casting is performed and the variable is returned as a sol::object. Otherwise, the variable is cast to T using sol::object::as<T>().
     * @param key The name of the variable
     * @return The queried variable, cast to T if T is not sol::object
     * 
     * @throws std::runtime_error if the variable is not found in the environment
     */
    template <typename T=sol::object>
    T get(std::string const& key)
    {
        auto ret = m_environment[key];
        if (!ret.valid()) {
            throw std::runtime_error("Key '" + key + "' not found in environment");
        }
        return ret;
    }


    /**
     * @brief get returns a variable stored in the active environment and checks if it is valid. If the variable is not found in the environment, an exception is thrown.
     * 
     * @tparam T the type to cast the variable to. If T is sol::object, no casting is performed and the variable is returned as a sol::object. Otherwise, the variable is cast to T using sol::object::as<T>().
     * @param key The name of the variable
     * @return The queried variable, cast to T if T is not sol::object
     * 
     * @throws std::runtime_error if the variable is not found in the environment
     */
    template <typename T=sol::object>
    T get(std::string const& key) const
    {
        auto ret = m_environment[key];
        if (!ret.valid()) {
            throw std::runtime_error("Key '" + key + "' not found in environment");
        }
        return ret;
    }

    /**
     * @brief get_feature returns a Feature stored in the active environment
     *
     * This is syntactic sugar for static_cast<DynamicFeature>(grunk["foo"]),
     * i.e. retrieval of the variable as a grunk::object and then casting it
     * to DynamicFeature
     *
     * @param key The name of the Feature
     * @return The queried feature
     */
    inline DynamicFeature get_feature(std::string const& key)
    {
        return get<DynamicFeature>(key);
    }

    /**
     * @brief get_feature returns a Feature stored in the active environment
     *
     * This is syntactic sugar for static_cast<DynamicFeature>(grunk["foo"]),
     * i.e. retrieval of the variable as a grunk::object and then casting it
     * to DynamicFeature
     *
     * @param key The name of the Feature
     * @return The queried feature
     */
    inline DynamicFeature get_feature(std::string const& key) const
    {
        return get<DynamicFeature>(key);
    }

    /**
     * @brief sets the id of every feature in the active environment to 
     * its LUA variable name. This is particularly useful for serialization
     */
    inline void tag_features()
    {
        auto tag_feature = [](sol::object key, sol::object value) {
            if (key.is<std::string>() && value.is<DynamicFeature>()) {
                value.as<DynamicFeature&>().set_id(key.as<std::string const&>());
            }
        };
        m_environment.for_each(tag_feature);
    }

    /**
     * @brief eval evaluates a LUA script in the active environment
     * @param lua_script The lua script to be evaluated
     */
    inline auto eval(std::string const& lua_script) {
        sol::state_view lua(m_environment.lua_state());
        return lua.safe_script(lua_script, m_environment);
    }

protected:
    /// @brief The wrapped Lua environment
    sol::environment m_environment;
};

} // namespace grunk