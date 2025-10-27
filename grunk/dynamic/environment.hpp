#pragma once 

#include <sol/sol.hpp>

namespace grunk {

class environment 
{
public:
    environment(sol::environment const& env) : m_environment(env) {}

    /**
     * @brief returns a variable stored in the active environment
     * @param key The name of the variable
     * @return The queried variable
     */
    inline sol::object operator[](std::string const& key)
    {
        return m_environment[key];
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
        DynamicFeature ret = m_environment[key];
        return ret;
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

private:
    sol::environment m_environment;
};

} // namespace grunk