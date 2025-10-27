#pragma once 

#include "grunk/dynamic/internal/parametric_core.hpp"
#include <object.hpp>
#include "grunk/dynamic/internal/sol_helpers.hpp"

namespace grunk {

template <typename Derived, typename T>
struct FeatureBase : public parametric::param<T>
{
public:
    FeatureBase(T const& v)
    : parametric::param<T>(v, "") {}

    explicit FeatureBase(parametric::param<T> const& p) : parametric::param<T>(p) {}

    Derived& with_id(std::string const& id) {
        this->set_id(id);
        return static_cast<Derived&>(*this);
    }

    bool operator==(Derived const& other) {
        return this->node_pointer() == other.node_pointer();
    }
};

template <typename T>
struct Feature : public FeatureBase<Feature<T>, T> 
{
    Feature(T const& v)
    : FeatureBase<Feature<T>, T>(v)
    {}

    explicit Feature(parametric::param<T> const& p) : FeatureBase<Feature<T>,T>(p) {}

    void set_value(T const& t) {
        this->change_value() = t;
    }

};

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

    template <typename T>
    void set_value(T const& t) {
        if constexpr (std::is_same_v<T, object>) {
            this->change_value() = t;
        } else {
            if (lua == nullptr) {
                // need to evalatue
                lua = value().lua_state();
            }
            this->change_value() = sol::make_object(lua, t);
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
        sol::table usertype_table = details::lookup_nested(l["environments"]["decorated"], usertype);
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


//TODO: deprecate this? Whats the point?
template <typename T>
Feature<T> feature(T const& v)
{
    return Feature<T>(v);
}

namespace details {

    /**
    * @brief is_feature_v returns false if the input type is not a Feature template realization
    * 
    * @tparam typename any ol' type
    */
    template<typename> constexpr bool is_feature_v = false;

    /**
    * @brief is_feature_v returns true, if the input template argument is a Feature template realization
    * 
    * @tparam T the element type of the Feature
    */
    template<typename T>
    constexpr bool is_feature_v<Feature<T>> = true;


    template <typename T>
    auto to_feature(T const& arg)
    {
        if constexpr (details::is_feature_v<T>){
            return arg;
        } else {
            return grunk::feature<T>(arg);
        }
    };

} // namespace details


} // namespace grunk
