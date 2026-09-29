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
            using R = std::decay_t<typename function_traits<DecayedF>::return_type>;
            if constexpr (std::is_void_v<R> || is_tuple_v<R>) {
                return std::nullopt;
            } else if constexpr (sol::is_unique_usertype_v<R>) {
                // R is itself a unique-ownership smart pointer registered via
                // sol::unique_usertype_traits<R> (e.g. OCCT's Handle(T) ==
                // opencascade::handle<T>) - the registered usertype (and hence the
                // type a colon-call needs to look its methods up under) is the
                // pointee, not the pointer wrapper itself.
                return std::type_index(typeid(sol::unique_usertype_element_t<R>));
            } else {
                return std::type_index(typeid(R));
            }
        } else {
            return std::nullopt;
        }
    }

    /**
     * @brief Deduces, at registration time, whether a callable is a genuine
     * mutator in the sense that matters for decorated/parametric dispatch (see
     * ActionDynamic.hpp's make_dynamic_action): evaluation there is lazy and
     * pull-based (grunk wraps the external `parametric` library's own DAG
     * engine), so a mutating call's own result feature, if never explicitly
     * read, is simply never evaluated at all - the underlying C++ mutation
     * never happens, silently.
     *
     * Deliberately requires BOTH of:
     *   1. a non-const lvalue-reference receiver ("self") - the call can
     *      actually write through it at all, and
     *   2. a `void` return type - the call has no OTHER possible reason to
     *      exist than that mutation, since there's nothing else to read.
     * Checking only (1) produces real false positives: plenty of ordinary,
     * non-mutating methods simply aren't marked `const` (this project's own
     * MyScalar::pow test fixture is exactly such a case - non-const, returns a
     * new MyScalar, mutates nothing) - flagging those would reject entirely
     * safe, already-working code. A non-const method that also returns
     * something meaningful gets no protection from this check at all (a real,
     * accepted gap, not an oversight): if that return value is itself read,
     * the call evaluates correctly regardless of what self-mutation may also
     * have happened, so there's no reliable way to tell "self-mutation was
     * this call's only point" apart from "the return value just wasn't read
     * this time" without knowing the future. A void-returning, non-const call
     * has no such ambiguity - it can only ever have been made for its side
     * effect - so this is the narrowest condition that reliably captures the
     * genuinely broken case without rejecting merely-not-const-qualified pure
     * methods.
     *
     * Same introspection limits as deduce_return_type_hint: only meaningful for
     * a callable shape function_traits.hpp can introspect (a function pointer,
     * member pointer, or a concrete, non-generic functor/lambda) with at least
     * one parameter. Returns false (i.e. "not detected as mutating", the
     * conservative/permissive default) for anything else - notably a
     * sol::overload_set (used pervasively by generated bindings for any
     * overloaded method), which has no single introspectable operator() at
     * all. An overloaded mutating method therefore currently gets no
     * protection from this mechanism at all; only a single, non-overloaded
     * mutating registration does.
     */
    template <typename F>
    bool deduce_receiver_is_mutating() {
        using DecayedF = std::decay_t<F>;
        // Nested if constexpr blocks throughout, not conditions joined by &&:
        // function_traits<DecayedF>::arity/return_type/argument<0> all have to be
        // named to even form a combined condition's own type, which would
        // force-instantiate function_traits<DecayedF> for a type like
        // sol::overload_set that has_function_traits_v<DecayedF> is specifically
        // there to rule out first - the exact hazard has_return_type's own doc
        // comment above already warns about, hit for real (a hard compile error,
        // not a SFINAE failure) when this was first written with joined
        // conditions. Nesting keeps each subsequent check's own body
        // uninstantiated whenever an earlier branch is false.
        if constexpr (has_function_traits_v<DecayedF>) {
            if constexpr (function_traits<DecayedF>::arity >= 1) {
                using Self = typename function_traits<DecayedF>::template argument<0>::type;
                constexpr bool self_is_mutable_ref =
                    std::is_lvalue_reference_v<Self> && !std::is_const_v<std::remove_reference_t<Self>>;
                if constexpr (self_is_mutable_ref) {
                    using R = typename function_traits<DecayedF>::return_type;
                    return std::is_void_v<R>;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
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

    /**
     * @brief the C++ type this function is a registered member function of, if any -
     * std::nullopt for constructors, free functions and operators (anything not
     * registered via usertype_proxy::add_member_function, which is the only place
     * this gets stamped, always with the exact usertype T it's called on). Lets
     * ActionDynamic::serialize() recognize a genuine member-function call and, if its
     * first argument's own producing function returns that same type, serialize it
     * using colon-call syntax (`arg:method(...)`) instead of the qualified
     * `Type.method(arg, ...)` form - see ActionDynamic.hpp.
     */
    std::optional<std::type_index> const& receiver_type_hint() const {
        return m_receiver_type_hint;
    }

    /**
     * @brief sets the receiver type hint - see receiver_type_hint(). Used exclusively
     * by usertype_proxy::add_member_function, right after construction, the same way
     * add_constructors stamps set_return_type_hint.
     */
    void set_receiver_type_hint(std::type_index type) {
        m_receiver_type_hint = type;
    }

    /**
     * @brief whether this function's own receiver ("self") is mutated by the call -
     * see details::deduce_receiver_is_mutating for the full explanation of why this
     * matters (a mutating call dispatched through the decorated/parametric
     * environment's lazy, pull-based evaluation silently never runs at all unless
     * its own result is explicitly read) and its own documented false-negative gap
     * for overloaded (sol::overload_set) registrations. Defaults to false (not
     * mutating) for any function_meta not built via usertype_proxy::add_member_function,
     * the only place this gets stamped.
     */
    bool receiver_is_mutating() const {
        return m_receiver_is_mutating;
    }

    /**
     * @brief sets whether this function's receiver is mutating - see
     * receiver_is_mutating(). Used exclusively by usertype_proxy::add_member_function,
     * right after construction, the same way set_receiver_type_hint is.
     */
    void set_receiver_is_mutating(bool mutating) {
        m_receiver_is_mutating = mutating;
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

    ///@brief the C++ type this is a registered member function of, if any - see receiver_type_hint()
    std::optional<std::type_index> m_receiver_type_hint;

    ///@brief whether this function's receiver is mutated by the call - see receiver_is_mutating()
    bool m_receiver_is_mutating {false};
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
