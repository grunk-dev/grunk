#pragma once 

#include <string>

namespace grunk {

namespace helper {

class String 
{
public:
    String() = default;
    String(std::string_view s);
    operator const char*() const;
    operator const std::string&() const;
private:
    std::string str;
};

} // namespace helper

} // namespace grunk
