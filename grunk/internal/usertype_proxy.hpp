#pragma once

#include <sol/sol.hpp>

namespace grunk {

template <typename T>
struct usertype_proxy {

    template <typename... Ctors>
    usertype_proxy& add_constructors() {
        ut["new"] = sol::constructors<Ctors...>();
        return *this;
    }

    template <typename... Ctors>
    usertype_proxy& add_bases() {
        ut[sol::base_classes] = sol::bases<Ctors...>();
        return *this;
    }

    template <typename F>
    usertype_proxy& add_member_function(std::string const& memfun_name, F&& fun) {
        ut[memfun_name] = fun;
        return *this;
    }

    template <typename F>
    usertype_proxy& add_data_member(std::string const& memfun_name, F&& fun) {
        //TODO: via sol::property? Differentiate readonly types?
        ut[memfun_name] = fun;
        return *this;
    }

    sol::usertype<T> ut;
};

} // namespace grunk
