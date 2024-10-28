#pragma once 

#include "parametric/core.hpp"
#include <object.hpp>

namespace grunk {

template <typename Derived, typename T>
struct FeatureBase : public parametric::param<T>
{
public:
    FeatureBase(T const& v)
    : parametric::param<T>(v, "") {}

    Derived& with_id(std::string const& id) {
        this->set_id(id);
        return static_cast<Derived&>(*this);
    }
};

template <typename T>
struct Feature : public FeatureBase<Feature<T>, T> 
{
    Feature(T const& v)
    : FeatureBase<Feature<T>, T>(v)
    {}

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

    template <typename T>
    void set_value(T const& t) {
        if constexpr (std::is_same_v<T, object>) {
            this->change_value() = t;
        } else {
            check_lua();
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
        sol::table usertype_table = l[usertype];
        return as(usertype_table);
    }

    bool operator==(Feature const& other) {
        return this->node_pointer() == other.node_pointer();
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
            return grunk::feature<T>(arg); //TODO: Until we properly support unnamed features, this will be an empty string
        }
    };

} // namespace details


// define operators

template <typename L, typename R>
Feature<decltype(std::declval<L>() + std::declval<R>())> operator+(Feature<L> const& l, Feature<R> const& r) {
    return grunk::action([](L const& lhs, R const& rhs){ return lhs+rhs; }, l, r).output();
}

} // namespace grunk
