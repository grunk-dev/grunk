#pragma once

#include "io.hpp"

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
