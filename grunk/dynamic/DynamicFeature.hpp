#pragma once 

#include "grunk/core/Feature.hpp"
#include <object.hpp>
#include "grunk/dynamic/sol_helpers.hpp"

namespace grunk {

template <>
class Feature<object> : public FeatureBase<Feature<object>, object>
{
    using Base = FeatureBase<Feature<object>, object>;

public:

    Feature(object const& v)
     : Base(v)
     , lua(v.lua_state())
    {}

    explicit Feature(parametric::param<object> const& p)
     : FeatureBase<Feature<object>,object>(p)
     , lua(nullptr) //TODO: This might be a problem. But we can't extract the lua state from the object without evaluating
    {}

    Feature(lua_State* lua_state = nullptr)
     : FeatureBase<Feature<object>, object>()
     , lua(lua_state)
    {}

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

    sol::table as(std::string const& usertype) const
    {
        check_lua();
        sol::state_view l(lua);
        sol::table usertype_table = details::lookup_nested(l["grunk"]["parametric_env"], usertype);
        return as(usertype_table);
    }

    lua_State* lua_state() const {
        return lua;
    }

private:

    void check_lua() const {
        if (!lua) {
            throw std::runtime_error("DynamicFeature: lua state is uninitialized");
        }
    }

    lua_State* lua;
};

using DynamicFeature = Feature<object>;


} // namespace grunk
