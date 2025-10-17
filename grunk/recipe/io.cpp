#include "io.hpp"
#include "grunk/core/state.hpp"

namespace {

    std::string to_string(double v) {
        char buf[64]{};
        std::to_chars_result result = std::to_chars(buf, buf + 64, v, std::chars_format::general);
        if (result.ec != std::errc()) {
            throw grunk::io_error("Error serializing double value. " + std::make_error_code(result.ec).message());
        } else {
            return std::string(buf, result.ptr - buf);
        }
    };

} // anonymouns namespace

namespace grunk {

std::string serialize(grunk::object const& v)
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

grunk::object deserialize(grunk::state& grunk, std::string const& v)
{
    auto ret = grunk.eval("return " + v);
    if (ret.valid()) {
        return ret;
    } else {
        throw grunk::io_error("Error deserializing \"" + v + "\".");
    }
}

} // namespace grunk