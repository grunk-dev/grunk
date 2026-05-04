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
    std::optional<std::string> name {std::nullopt};
    //TODO: type information?
    //TODO: default value?
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
    function_meta(
        std::string const& name_,
        std::optional<std::vector<Parameter>> const& params_,
        sol::function const& func_)
     : name(name_)
     , params(params_)
     , func(func_)
    {}

    std::string const& get_name() const {
        return name;
    }

    std::optional<std::vector<Parameter>> const& get_params() const {
        return params;
    }

    sol::protected_function const& get_function() const {
        return func;
    }

    template <typename... Args>
    sol::protected_function_result call(Args&&... args) const {
        return func(std::forward<Args>(args)...);
    }

    decltype(auto) operator()(sol::variadic_args va) const {
        return func(va);
    }

    decltype(auto) lua_state() const {
        return func.lua_state();
    }

private:
    std::string name;
    std::optional<std::vector<Parameter>> params;
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
