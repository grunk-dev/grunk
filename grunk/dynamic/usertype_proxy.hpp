// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <sol/sol.hpp>
#include "grunk/dynamic/function_meta.hpp"

#include <typeindex>

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
        sol::object func = create_function_meta(ut.lua_state(), name + ".new", overload);
        // A constructor's result is always exactly T - stamp the type hint directly
        // rather than relying on deduce_return_type_hint, which can't introspect the
        // sol::overload_set every constructor call gets wrapped in (see
        // function_meta::set_return_type_hint). This is what lets native colon-call
        // dispatch (DynamicFeature's __index fallback) work right after construction,
        // e.g. `MyScalar.new_feature(2):pow(3)`.
        func.as<function_meta&>().set_return_type_hint(std::type_index(typeid(T)));
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
     *
     * @note If `fun` is itself a `sol::overload(...)` set, the resulting function_meta
     *       gets no return-type hint (see function_meta::return_type_hint /
     *       details::deduce_return_type_hint), the same reason constructors would too if
     *       add_constructors didn't stamp theirs manually via
     *       function_meta::set_return_type_hint - a sol::overload_set isn't
     *       introspectable via function_traits. A DynamicFeature returned by such a
     *       method then can't use native colon-call dispatch (see DynamicFeature::call /
     *       the Feature usertype's sol::meta_function::index handler in state.hpp) and
     *       needs an explicit :as(Type) call or the qualified TypeName.method(...) form
     *       instead. If this becomes a real need, call func.as<function_meta&>()
     *       .set_return_type_hint(...) on the result the same way add_constructors does.
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
        sol::object func = create_function_meta(ut.lua_state(), fun_name, params, std::forward<F>(fun));
        // A member function's receiver is always exactly T (the usertype it's being
        // added to) - stamp it unconditionally, the same way add_constructors stamps
        // its own return type hint, so ActionDynamic::serialize() can recognize this
        // call as a genuine method call later (see function_meta::receiver_type_hint).
        func.as<function_meta&>().set_receiver_type_hint(std::type_index(typeid(T)));

        ut.set(std::forward<Key>(key), func);
        return *this;
    }

    /**
     * @brief Adds a data member to the usertype. This function allows for adding data members to the usertype, which can be accessed from Lua on instances of the type. The function metadata is created using the provided name and parameters, which can be used for error messages and for documentation purposes.
     *
     * @tparam Key The type of the key used to access the data member in Lua, e.g. std::string or const char*.
     * @tparam F The type of the data member to be added.
     * @param memfun_name The key used to access the data member in Lua.
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
     * @brief Adds support for std::vector<VecElement> (VecElement defaults to T, the usertype itself) to the usertype. This function adds two member functions to the usertype: "as_vec" and "new_vec". The "as_vec" function can be called from Lua with either a table or variadic arguments to create a std::vector<VecElement> from the provided values. The "new_vec" function can be called from Lua to create an empty std::vector<VecElement>.
     *
     * Each element is converted to VecElement by calling through an ordinary, real
     * sol2 function (see convert_one below) rather than via
     * sol::object::is<VecElement>()/.as<VecElement>() directly. This matters
     * whenever T participates in a registered inheritance relationship
     * (add_bases<T>() on some derived type Derived): sol2's own argument-binding
     * machinery for an ordinary bound function's parameters
     * (stack_get_unqualified.hpp's qualified_getter, backed by inheritance.hpp's
     * type_cast/type_unique_cast) correctly performs the derived-to-base cast, but
     * the generic sol::object::is<T>()/.as<T>() API does not walk that same
     * inheritance chain - a Derived-typed value was previously rejected by
     * as_vec/new_vec even though it binds correctly as a genuine T-typed function
     * argument elsewhere. Routing through a real function call reuses the code path
     * that is already correct.
     *
     * The explicit VecElement template parameter is what lets this support
     * std::vector<Handle<T>> for a sol::unique_usertype_traits-wrapped smart
     * pointer around T (e.g. OCCT's Handle(T)/opencascade::handle<T>) - the same
     * unique-usertype-aware cast (type_unique_cast) that already lets a single
     * Handle(Derived)-returning function argument bind correctly to a
     * Handle(Base) const& parameter applies per-element here too, once the
     * identity function below is itself typed as VecElement(VecElement const&)
     * rather than T(T const&). Call e.g. `.with_std_vector<Handle<T>>()` on T's own
     * usertype_proxy to get `T.as_vec(...)` producing std::vector<Handle<T>>
     * instead of std::vector<T>.
     *
     * @tparam VecElement the std::vector element type - T itself by default, or a
     *         smart-pointer-like wrapper around T (see above).
     * @return A reference to the usertype_proxy for chaining.
     */
    template <typename VecElement = T>
    usertype_proxy& with_std_vector() {

        std::string ud_name = name;
        sol::state_view lua(ut.lua_state());

        // A trivial real function VecElement(VecElement const&) - calling it forces
        // sol2's ordinary, inheritance-aware argument-binding path onto whatever
        // value is passed in, rather than the generic (and not inheritance-aware)
        // sol::object::as<VecElement>().
        sol::protected_function cast_to_element = sol::make_object(lua, [](VecElement const& x) -> VecElement { return x; });

        auto convert_one = [ud_name, cast_to_element](sol::object const& v) -> VecElement {
            sol::protected_function_result res = cast_to_element(v);
            if (!res.valid()) {
                throw std::runtime_error("Cannot create std::vector. The values cannot be converted to the expected usertype \"" + ud_name + "\".");
            }
            return res.get<VecElement>();
        };

        auto from_varargs = [convert_one](sol::variadic_args va){
            std::vector<VecElement> ret;
            ret.reserve(va.size());
            for (auto const& v : va) {
                ret.push_back(convert_one(v));
            }
            return ret;
        };

        auto from_table = [convert_one](sol::table t) {
            std::vector<VecElement> ret;
            ret.reserve(t.size());
            for (auto const& kv : t) {
                ret.push_back(convert_one(kv.second));
            }
            return ret;
        };

        add_member_function("as_vec", sol::overload(from_table, from_varargs));
        add_member_function("new_vec", [](){ return std::vector<VecElement>{}; });

        return *this;
    }

    /// @brief the name of the type
    std::string name;

    /// @brief the wrapped sol::usertype instance
    sol::usertype<T> ut;
};

} // namespace grunk
