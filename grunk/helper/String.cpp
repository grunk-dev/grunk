#include "String.hpp"

namespace grunk {

namespace helper {


String::String(std::string_view s) : str{s} {}

String::operator const char*() const {
    return str.c_str();
}

String::operator const std::string() const {
    return str;
}


} // namespace helper

} // namespace grunk
