#pragma once

#include <sol/sol.hpp>

namespace grunk {

template <typename T>
std::vector<T> to_std_vector(sol::variadic_args va)
{
    std::vector<T> ret;
    ret.reserve(va.size());
    for (auto const& v : va) {
        if (!v.is<T>()) {
            throw std::runtime_error("Cannot create std::vector. The values cannot be converted to the expected usertype.");
        }
        ret.push_back(v.as<T const&>());
    }
    return ret;
}

template <typename T>
std::vector<T> to_std_vector(sol::table const& t)
{
    std::vector<T> ret;
    ret.reserve(t.size());
    for (auto const& kv : t) {
        if (!kv.second.is<T>()) {
            throw std::runtime_error("Cannot create std::vector from table. The values cannot be converted to the expected usertype.");
        }
        ret.push_back(kv.second.as<T>());
    }
    return ret;
}

inline sol::object as_vec(sol::variadic_args va)
{
    if (va.size() < 1) {
        return sol::nil;
    }
    sol::object first = va[0];
    sol::state_view lua(first.lua_state());
    switch (first.get_type()) {
        case sol::type::boolean:
            return sol::make_object(lua, to_std_vector<bool>(va));
        case sol::type::string:
            return sol::make_object(lua, to_std_vector<std::string>(va));
        case sol::type::number:
            return sol::make_object(lua, to_std_vector<double>(va)); //TODO: Can't differentiate float, double, int here
        case sol::type::userdata: {
            if (!first.as<sol::table const&>()["as_vec"].valid()) {
                throw std::runtime_error("Error in as_vec: Detected userdata, but no as_vec method. Have you registered your type with the \"with_std_vector\" function?");
            }
            return first.as<sol::table const&>()["as_vec"](va);
        }
        default:
            throw std::runtime_error("Error in as_vec: Cannot convert the given type to a std::vector.");
    }
    return sol::nil;
}

inline sol::object as_vec(sol::table const& t)
{
    if (t.size() < 1) {
        return sol::nil;
    }
    sol::object first = (*t.begin()).second;
    sol::state_view lua(first.lua_state());
    switch (first.get_type()) {
        case sol::type::boolean:
            return sol::make_object(lua, to_std_vector<bool>(t));
        case sol::type::string:
            return sol::make_object(lua, to_std_vector<std::string>(t));
        case sol::type::number:
            return sol::make_object(lua, to_std_vector<double>(t)); //TODO: Can't differentiate float, double, int here
        case sol::type::userdata: {
            if (!first.as<sol::table const&>()["as_vec"].valid()) {
                throw std::runtime_error("Error in as_vec: Detected userdata, but no as_vec method. Have you registered your type with the \"with_std_vector\" function?");
            }
            return first.as<sol::table const&>()["as_vec"](t);
        }
        default:
            throw std::runtime_error("Error in as_vec: Cannot convert the given type to a std::vector.");
    }
    return sol::nil;
}

} // namespace grunk