#pragma once

#include "grunk/dynamic/object.hpp"
#include "grunk/dynamic/io_error.hpp"

#include <charconv>
#include <system_error>

namespace grunk {


    inline std::string to_string(double v) {
        char buf[64]{};
        std::to_chars_result result = std::to_chars(buf, buf + 64, v, std::chars_format::general);
        if (result.ec != std::errc()) {
            throw grunk::io_error("Error serializing double value. " + std::make_error_code(result.ec).message());
        } else {
            return std::string(buf, result.ptr - buf);
        }
    };

    inline std::string serialize(grunk::object const& v)
    {
        std::string err_msg_prefix = "Error serializing grunk::object to string: ";

        switch (v.get_type()) {
            case sol::type::nil:
                return "nil";
            break;
            case sol::type::boolean:
                return v.as<bool>()? "true" : "false";
            break;
            case sol::type::string:
                return std::string("\"") + v.as<std::string>() + "\"";
            break;
            case sol::type::number: {

                double d = v.as<double>();

                // handle int and double cases differently
                double intpart;
                if (std::modf(d, &intpart) == 0.0) {
                    // the number is an integer
                    return std::to_string((int)intpart);
                } else {
                    // the number is a double value
                    return to_string(d);
                }
                break;
            }
            case sol::type::userdata: {
                sol::table ud = v.as<sol::table>();
                std::string name = ud["__name"];
                sol::object serialize;
                try {
                    serialize = ud["serialize"]; //TODO this panicks for unregistered data! It doesn't throw and the code crashes. What to do?
                } catch (sol::error) {
                    throw grunk::io_error("Cannot serialize an opaque type \"" + name + "\". Make sure registery your type and add a serialization method.");
                }
                if (!serialize.valid() || !ud["serialize"].is<sol::protected_function>()) {
                    throw grunk::io_error(err_msg_prefix + "Serialization method is not available for " + name + ".");
                }
                auto result = serialize.as<sol::protected_function>()(v);
                if (result.valid()) {
                    return result.get<std::string>();
                } else {
                    throw grunk::io_error(err_msg_prefix + "Error invoking Serialization method for " + name + ".");
                }
                break;
            }
            default:
                throw grunk::io_error(err_msg_prefix + "Serialization not supported for sol::objects of the given type.");
        }
        return "";
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


} // namespace parametric

#include <parametric/core.hpp>
