#pragma once

#include <sol/sol.hpp>

namespace grunk {

template <typename T>
struct usertype_proxy {

    usertype_proxy(std::string const& name_, sol::usertype<T> const& ut_)
     : name(name_)
     , ut(ut_)
    {
        sol::state_view lua = ut.lua_state();
        sol::table registry = lua["grunk"]["registry"];
        registry[name] = lua.create_table();
    }

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

    template <typename Key, typename F>
    usertype_proxy& add_member_function(Key&& key, F&& fun, std::vector<Parameter> params = {}) {
        ut.set_function(std::forward<Key>(key), std::forward<F>(fun));

        std::string fun_name;
        if constexpr (std::is_convertible_v<Key, std::string>) {
            fun_name = std::forward<Key>(key);
        } else {
            fun_name = sol::to_string(std::forward<Key>(key));
        }

        fun_name = name + "." + fun_name;
        sol::state_view lua = ut.lua_state();
        sol::reference const& f = ut[std::forward<Key>(key)];
        register_metadata(f, fun_name, params);

        return *this;
    }

    template <typename F>
    usertype_proxy& add_data_member(std::string const& memfun_name, F&& fun) {
        //TODO: via sol::property? Differentiate readonly types?
        ut[memfun_name] = std::forward<F>(fun);
        return *this;
    }

    std::string name;
    sol::usertype<T> ut;
};

} // namespace grunk
