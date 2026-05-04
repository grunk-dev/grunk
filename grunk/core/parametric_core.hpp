// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

/**
 * @file Feature.hpp
 *
 * Declaration and Definition of the Feature class.
 *
 * @defgroup core
 * @defgroup advanced
 */

#pragma once

#ifdef GRUNK_WITH_DYNAMIC

#include "grunk/dynamic/object.hpp"
#include "grunk/dynamic/io_error.hpp"

#include <charconv>
#include <system_error>

namespace grunk {

    class Recipe;

    inline std::string to_string(double v) {
        char buf[64]{};
        std::to_chars_result result = std::to_chars(buf, buf + 64, v, std::chars_format::general);
        if (result.ec != std::errc()) {
            throw grunk::io_error("Error serializing double value. " + std::make_error_code(result.ec).message());
        } else {
            std::string s(buf, result.ptr - buf);
            
            // only append ".0" for finite numeric representations that lack '.' or exponent
            if (std::isfinite(v) &&
                s.find('.') == std::string::npos &&
                s.find('e') == std::string::npos &&
                s.find('E') == std::string::npos) {
                s += ".0";
            }

            return s;
        }
    };

    inline std::string serialize(grunk::object const& v)
    {
        sol::state_view lua(v.lua_state());
        std::string err_msg_prefix = "Error serializing grunk::object to string: ";

        // for userdata, we want to throw if there is no tostring metamethod
        if (v.get_type() == sol::type::userdata) {
            sol::table ud = v.as<sol::table>();
            std::string name = ud["__name"];
            sol::object serialize;
            try {
                serialize = ud[sol::meta_function::to_string]; //TODO this panicks for unregistered data! It doesn't throw and the code crashes. What to do?
            } catch (sol::error) {
                throw grunk::io_error("Cannot serialize an opaque type \"" + name + "\". Make sure registery your type and add a tostring metamethod.");
            }
            if (!serialize.valid() || !ud[sol::meta_function::to_string].is<sol::protected_function>()) {
                throw grunk::io_error(err_msg_prefix + "tostring metamethod is not available for " + name + ".");
            }
        }

        sol::protected_function tostring_func = lua["tostring"];
        sol::protected_function_result tostring_result = tostring_func(v);
        if (tostring_result.valid()) {
            if (v.get_type() == sol::type::string) {
                // wrap strings in quotes
                return "\"" + tostring_result.get<std::string>() + "\"";
            } else {
                return tostring_result.get<std::string>();
            }
        } else {
            sol::error err = tostring_result;
            throw grunk::io_error(err_msg_prefix + "Lua error in tostring: " + err.what());
        }
        throw grunk::io_error(err_msg_prefix + "Unknown error.");
    }

} // namespace grunk

namespace parametric {

template <typename T>
std::string serialize(T const&);


/**
 * @brief template specialization of parametric::serialize for grunk::object
 *
 * @tparam  empty -> this is a full template specialization
 * @param v The value to be serialized
 * @return std::string string representation of the yaml node
 */
template <>
inline std::string serialize(grunk::object const& v)
{
    return grunk::serialize(v);
}

template <>
inline std::string serialize(int const& v)
{
    return std::to_string(v);
}

template <>
inline std::string serialize(double const& v)
{
    return grunk::to_string(v);
}

template <>
inline std::string serialize(std::string const& v)
{
    return v;
}

template <>
inline std::string serialize(bool const& v)
{
    return v ? "true" : "false";
}

// This is needed, because grunk::Recipes are the root inputs
// of recipe call actions. A feature that serializes to an 
// empty string is ignored in Serializer
template <>
inline std::string serialize(grunk::Recipe const&)
{
    return "";
}

} // namespace parametric

#endif // GRUNK_WITH_DYNAMIC

#include <parametric/core.hpp>
