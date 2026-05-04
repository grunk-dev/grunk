// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <sol/sol.hpp>
#include "grunk/dynamic/function_meta.hpp"

namespace grunk {

/**
 * @brief A proxy class for sol::usertype that allows for more convenient registration of usertypes with metadata. It provides a fluent interface for adding constructors, member functions and data members to the usertype, as well as setting arbitrary properties. It also provides a helper function with_std_vector to add support for std::vector<T> where T is the usertype itself.
 *
 * @tparam T the C++ type that the usertype represents
 */
template <typename T>
struct usertype_proxy {

    /**
     * @brief constructs a usertype_proxy with the given name and sol::usertype. The name is used for creating function metadata and for error messages. The sol::usertype is the actual usertype that is being registered with Lua, and it is stored as a member of the proxy to allow for convenient access when adding constructors, member functions and data members.
     * 
     * @param name_ the name of the usertype, used for creating function metadata and for error messages
     * @param ut_ the sol::usertype that is being registered with Lua
     */
    usertype_proxy(std::string const& name_, sol::usertype<T> const& ut_)
     : name(name_)
     , ut(ut_)
    {}

    /**
     * @brief Adds constructors to the usertype. This function allows for adding multiple constructors to the usertype, which can be called from Lua to create instances of the type.
     *
     * @tparam Ctors The types of the constructors to be added.
     * @param ctors The constructors to be added.
     * @return A reference to the usertype_proxy for chaining.
     */
    template <typename... Ctors>
    usertype_proxy& add_constructors(Ctors&&... ctors) {
        auto overload = sol::overload(std::forward<Ctors>(ctors)...) ;
        auto func = create_function_meta(ut.lua_state(), name + ".new", overload);
        ut[sol::meta_function::construct] = func;
        return *this;
    }

    /**
     * @brief Adds base classes to the usertype. This function allows for specifying base classes for the usertype, which enables Lua to recognize the inheritance hierarchy and allows for upcasting and downcasting between the usertype and its base classes.
     *
     * @tparam Ctors The types of the base classes to be added.
     * @return A reference to the usertype_proxy for chaining.
     */
    template <typename... Ctors>
    usertype_proxy& add_bases() {
        ut[sol::base_classes] = sol::bases<Ctors...>();
        return *this;
    }

    /**
     * @brief Adds a member function to the usertype. This function allows for adding member functions to the usertype, which can be called from Lua on instances of the type. The function metadata is created using the provided name and parameters, which can be used for error messages and for documentation purposes.
     *
     * @tparam Key The type of the key used to access the member function in Lua, e.g. std::string or const char*.
     * @tparam F The type of the member function to be added.
     * @param key The key used to access the member function in Lua.
     * @param fun The member function to be added.
     * @param params Optional vector of parameters for the member function, used for creating function metadata.
     * @return A reference to the usertype_proxy for chaining.
     */
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

    /**
     * @brief Adds a data member to the usertype. This function allows for adding data members to the usertype, which can be accessed from Lua on instances of the type. The function metadata is created using the provided name and parameters, which can be used for error messages and for documentation purposes.
     *
     * @tparam Key The type of the key used to access the data member in Lua, e.g. std::string or const char*.
     * @tparam F The type of the data member to be added.
     * @param key The key used to access the data member in Lua.
     * @param fun The data member to be added.
     * @return A reference to the usertype_proxy for chaining.
     */
    template <typename F>
    usertype_proxy& add_data_member(std::string const& memfun_name, F&& fun) {
        //TODO: via sol::property? Differentiate readonly types?
        ut[memfun_name] = std::forward<F>(fun);
        return *this;
    }

    /**
     * @brief Sets an arbitrary property on the usertype. This function allows for setting any property on the usertype, which can be used for various purposes such as adding custom metamethods or storing additional metadata.
     *
     * @tparam Args The types of the arguments to be passed to the set function.
     * @param args The arguments to be passed to the set function.
     * @return A reference to the usertype_proxy for chaining.
     */
    template <typename... Args>
    decltype(auto) set(Args... args) {
        ut.set(std::forward<Args>(args)...);
        return *this;
    }


    /**
     * @brief Adds support for std::vector<T> where T is the usertype itself. This function adds two member functions to the usertype: "as_vec" and "new_vec". The "as_vec" function can be called from Lua with either a table or variadic arguments to create a std::vector<T> from the provided values. The "new_vec" function can be called from Lua to create an empty std::vector<T>.
     *
     * @return A reference to the usertype_proxy for chaining.
     */
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
