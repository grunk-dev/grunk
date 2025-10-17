#pragma once 

#include "grunk/core/object.hpp"
#include "io_error.hpp"

#include <charconv>
#include <system_error>

namespace grunk {

    class state;

    std::string serialize(grunk::object const& v);

    grunk::object deserialize(grunk::state& grunk, std::string const& v);

} // namespace grunk