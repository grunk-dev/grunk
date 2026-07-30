// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

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
     */
    function_meta(
        std::string const& name_,
        std::optional<std::vector<Parameter>> const& params_,
        sol::function const& func_)
     : name(name_)
     , params(params_)
     , func(func_)
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

private:
    ///@brief the name of the function
    std::string name;

    ///@brief the parameters of the function
    std::optional<std::vector<Parameter>> params;

    ///@brief the wrapped Lua function
    sol::protected_function func;
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
    sol::object funobj = sol::make_object(lua, sol::as_function(std::forward<F>(func)));
    function_meta meta{name, params, funobj.as<sol::protected_function>()};
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
    sol::object funobj = sol::make_object(lua, sol::as_function(std::forward<F>(func)));
    function_meta meta{name, std::nullopt, funobj.as<sol::protected_function>()};
    return sol::make_object(lua, meta);
}

} // namespace grunk
