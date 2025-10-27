#pragma once

#include <sol/sol.hpp>
#include "grunk/dynamic/function_meta.hpp"

namespace grunk {

template <typename T>
struct usertype_proxy {

    usertype_proxy(std::string const& name_, sol::usertype<T> const& ut_)
     : name(name_)
     , ut(ut_)
    {}

    template <typename... Ctors>
    usertype_proxy& add_constructors(Ctors&&... ctors) {
        auto overload = sol::overload(std::forward<Ctors>(ctors)...) ;
        auto func = create_function_meta(ut.lua_state(), name + ".new", overload);
        ut[sol::meta_function::construct] = func;
        return *this;
    }

    template <typename... Ctors>
    usertype_proxy& add_bases() {
        ut[sol::base_classes] = sol::bases<Ctors...>();
        return *this;
    }

    template <typename Key, typename F>
    usertype_proxy& add_member_function(Key&& key, F&& fun, std::vector<Parameter> params = {}) {
        
        std::string fun_name;
        if constexpr (std::is_convertible_v<Key, std::string>) {
            fun_name = std::forward<Key>(key);
        } else {
            fun_name = sol::to_string(std::forward<Key>(key));
        }
        fun_name = name + "." + fun_name;
        auto func = create_function_meta(ut.lua_state(), fun_name, params, std::forward<F>(fun));
        
        ut.set(std::forward<Key>(key), func);
        return *this;
    }

    template <typename F>
    usertype_proxy& add_data_member(std::string const& memfun_name, F&& fun) {
        //TODO: via sol::property? Differentiate readonly types?
        ut[memfun_name] = std::forward<F>(fun);
        return *this;
    }

    usertype_proxy& with_std_vector() {

        std::string ud_name = name;
        auto from_varargs = [ud_name](sol::variadic_args va){
            std::vector<T> ret;
            ret.reserve(va.size());
            for (auto const& v : va) {
                if (!v.is<T>()) {
                    throw std::runtime_error("Cannot create std::vector. The values cannot be converted to the expected usertype \"" + ud_name + "\".");
                }
                ret.push_back(v.as<T const&>());
            }
            return ret;
        };

        auto from_table = [ud_name](sol::table t) {
            std::vector<T> ret;
            ret.reserve(t.size());
            for (auto const& kv : t) {
                if (!kv.second.is<T>()) {
                    throw std::runtime_error("Cannot create std::vector from table. The values cannot be converted to the expected usertype \"" + ud_name + "\".");
                }
                ret.push_back(kv.second.as<T>());
            }
            return ret;
        };

        add_member_function("as_vec", sol::overload(from_table, from_varargs));
        add_member_function("new_vec", [](){ return std::vector<T>{}; });

        return *this;
    }

    std::string name;
    sol::usertype<T> ut;
};

} // namespace grunk
