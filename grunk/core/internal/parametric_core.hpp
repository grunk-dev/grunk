#pragma once

#include "grunk/core/object.hpp"
#include "grunk/core/io_error.hpp"

#include <charconv>
#include <system_error>

namespace grunk {

    namespace details {

        inline std::string to_string(double v) {
            char buf[64]{};
            std::to_chars_result result = std::to_chars(buf, buf + 64, v, std::chars_format::general);
            if (result.ec != std::errc()) {
                throw grunk::io_error("Error serializing double value. " + std::make_error_code(result.ec).message());
            } else {
                return std::string(buf, result.ptr - buf);
            }
        };
    } // namespace details

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
                    return details::to_string(d);
                }
                break;
            }
            case sol::type::userdata: {
                auto ud = v.as<sol::userdata>();
                // std::cout << ud["__name"].as<std::string>() << "\n";
                sol::protected_function serialize = ud["serialize"];
                if (serialize.valid()) {
                    auto result = serialize(v);
                    if (result.valid()) {
                        return result.get<std::string>();
                    } else {
                        throw grunk::io_error(err_msg_prefix + "Serialization method is not available.");
                    }
                } else {
                    throw grunk::io_error(err_msg_prefix + "Serialization method is not available.");
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

} // namespace parametric

#include <parametric/core.hpp>
