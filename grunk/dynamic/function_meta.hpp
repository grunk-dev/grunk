// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/core/function_traits.hpp"

#include <optional>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace grunk {

/**
 * @brief Represents a parameter for a dynamic function.
 *
 * @ingroup advanced_dynamic
 */
struct Parameter
{
    /// @brief the name of the function parameter
    std::optional<std::string> name {std::nullopt};

    //TODO: type information?
    //TODO: default value?

    /// @brief wether the parameter is passed by const reference. Only meaningful for C++ types.
    bool is_const_reference {false};
};

namespace details {

    /**
     * @brief SFINAE detector: true iff F has a single, non-overloaded, non-generic
     * operator() whose address can be taken - i.e. it's a plain functor/non-generic
     * lambda, the shape function_traits.hpp's generic functor fallback expects.
     * False for callable-but-operator()-less types like sol::overload_set (which
     * dispatches via templates rather than exposing operator() at all) and for
     * overloaded/generic-lambda call operators (address-of is ambiguous/ill-formed).
     *
     * This has to be checked *before* ever naming function_traits<F>::return_type:
     * function_traits.hpp's generic fallback unconditionally does
     * decltype(&F::operator()) in its class body, which is a hard error (not a SFINAE
     * failure caught by void_t) for a type with no operator() at all - sol::overload_set
     * hit exactly this and broke the build.
     */
    template <typename F, typename = void>
    struct has_call_operator : std::false_type {};

    template <typename F>
    struct has_call_operator<F, std::void_t<decltype(&F::operator())>> : std::true_type {};

    /**
     * @brief true iff F is one of the callable shapes function_traits.hpp can safely
     * introspect without instantiating an ill-formed body: function pointers/references,
     * member function/data pointers (all of which match one of function_traits.hpp's
     * specific partial specializations), or a plain functor/non-generic lambda (which
     * falls through to the generic specialization, safe only because has_call_operator
     * already confirmed operator() exists and is unambiguous).
     */
    template <typename F>
    constexpr bool has_function_traits_v =
        std::is_function_v<std::remove_pointer_t<F>> ||
        std::is_member_pointer_v<F> ||
        has_call_operator<F>::value;

    /**
     * @brief SFINAE detector: true iff details::function_traits<F> exposes a
     * return_type member, i.e. F is one of the callable shapes function_traits.hpp
     * knows how to introspect (function pointers, member pointers, or a non-generic
     * functor/lambda with a concrete operator()).
     */
    template <typename F, typename = void>
    struct has_return_type : std::false_type {};

    template <typename F>
    struct has_return_type<F, std::enable_if_t<has_function_traits_v<F>>> : std::true_type {};

    /**
     * @brief Deduces, at registration time, the C++ return type of a callable about to
     * become a function_meta - i.e. the type its eventual result feature will end up
     * wrapping, well before anything is ever evaluated. This is what lets a
     * DynamicFeature carry a static type hint (see DynamicFeature::set_type_hint) for
     * native colon-call dispatch without forcing evaluation just to answer a type
     * query.
     *
     * Returns std::nullopt for a void or std::tuple (multi-output) return type, or for
     * callable shapes function_traits.hpp can't introspect (generic lambdas being the
     * main one - none of grunk's own registrations use them). In every "no hint" case,
     * dispatch simply falls back to requiring an explicit DynamicFeature::as() call or
     * the qualified TypeName.method(...) form - never to eager evaluation.
     */
    template <typename F>
    std::optional<std::type_index> deduce_return_type_hint() {
        using DecayedF = std::decay_t<F>;
        if constexpr (has_return_type<DecayedF>::value) {
            using R = typename function_traits<DecayedF>::return_type;
            if constexpr (std::is_void_v<R> || is_tuple_v<std::decay_t<R>>) {
                return std::nullopt;
            } else {
                return std::type_index(typeid(R));
            }
        } else {
            return std::nullopt;
        }
    }

} // namespace details

/**
 * @brief Represents metadata for a dynamic function.
 *
 * @ingroup advanced_dynamic
 */
class function_meta
{
public:

    /**
     * @brief construct a function_meta isntance
     *
     * @param name_ The name of the function
     * @param params_ The parameters of the function
     * @param func_ The Lua function
     * @param return_type_hint_ the C++ return type of the wrapped callable, if known
     *        statically at registration time (see details::deduce_return_type_hint) -
     *        std::nullopt if it isn't (void/tuple return, or an uninspectable callable
     *        shape like a generic lambda).
     */
    function_meta(
        std::string const& name_,
        std::optional<std::vector<Parameter>> const& params_,
        sol::function const& func_,
        std::optional<std::type_index> return_type_hint_ = std::nullopt)
     : name(name_)
     , params(params_)
     , func(func_)
     , m_return_type_hint(return_type_hint_)
    {}

    /**
     * @brief getter for the name of the function
     */
    std::string const& get_name() const {
        return name;
    }

    /**
     * @brief getter for the parameters of the function
     */
    std::optional<std::vector<Parameter>> const& get_params() const {
        return params;
    }

    /**
     * @brief getter for the wrapped Lua function
     */
    sol::protected_function const& get_function() const {
        return func;
    }

    /**
     * @brief call the function
     * 
     * @tparam Args the arguments passed as parameters
     */
    template <typename... Args>
    sol::protected_function_result call(Args&&... args) const {
        return func(std::forward<Args>(args)...);
    }

    /**
     * @brief call the function
     *
     * @param va the arguments passed as parameters
     *
     * @throws sol::error if the underlying call fails. func(va) alone would swallow such a
     * failure silently: a sol::protected_function_result stays a valid C++ value even when the
     * call it represents failed, so returning it unchecked (as this used to) lets the error
     * object (e.g. an exception's .what() text) flow back out as if it were func's actual,
     * successful return value - this is registered as function_meta's own operator() usertype
     * metamethod (state.hpp's init(), sol::meta_function::call), which sol2 calls under its own
     * protected dispatch, so throwing here converts back into a proper Lua-level error instead.
     */
    sol::protected_function_result operator()(sol::variadic_args va) const {
        sol::protected_function_result result = func(va);
        if (!result.valid()) {
            sol::error err = result;
            throw err;
        }
        return result;
    }

    /**
     * @brief a getter for the underlying lua state of the wrapped Lua function
     */
    decltype(auto) lua_state() const {
        return func.lua_state();
    }

    /**
     * @brief the C++ return type of the wrapped callable, known statically since
     * registration time - std::nullopt if none is available (see the constructor's
     * documentation and details::deduce_return_type_hint).
     */
    std::optional<std::type_index> const& return_type_hint() const {
        return m_return_type_hint;
    }

    /**
     * @brief overrides the return type hint after construction - used by
     * usertype_proxy::add_constructors, where the result type is always exactly the
     * usertype being registered (known unconditionally, not deduced): a constructor's
     * callable is always wrapped in sol::overload(...) even when there's only one of
     * them, and sol::overload_set isn't introspectable via function_traits, so the
     * normal deduce_return_type_hint path always yields std::nullopt for constructors.
     */
    void set_return_type_hint(std::type_index type) {
        m_return_type_hint = type;
    }

private:
    ///@brief the name of the function
    std::string name;

    ///@brief the parameters of the function
    std::optional<std::vector<Parameter>> params;

    ///@brief the wrapped Lua function
    sol::protected_function func;

    ///@brief the wrapped callable's C++ return type, if known statically - see return_type_hint()
    std::optional<std::type_index> m_return_type_hint;
};

/**
 * @brief Creates a function_meta instance from a function and its metadata.
 *
 * @ingroup advanced_dynamic
 */
template <typename F>
sol::object create_function_meta(
    sol::state_view lua,
    std::string const& name,
    std::vector<Parameter> const& params,
    F&& func)
{
    auto return_type_hint = details::deduce_return_type_hint<F>();
    sol::object funobj = sol::make_object(lua, sol::as_function(std::forward<F>(func)));
    function_meta meta{name, params, funobj.as<sol::protected_function>(), return_type_hint};
    return sol::make_object(lua, meta);
}

/**
 * @brief Creates a function_meta instance from a function and its name, without parameter metadata.
 *
 * @ingroup advanced_dynamic
 */
template <typename F>
sol::object create_function_meta(
    sol::state_view lua,
    std::string const& name,
    F&& func)
{
    auto return_type_hint = details::deduce_return_type_hint<F>();
    sol::object funobj = sol::make_object(lua, sol::as_function(std::forward<F>(func)));
    function_meta meta{name, std::nullopt, funobj.as<sol::protected_function>(), return_type_hint};
    return sol::make_object(lua, meta);
}

} // namespace grunk
