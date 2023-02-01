#pragma once

/*
 * IMPORTANT: Always include this header rather than <parametric/core.hpp>. This makes sure that
 * the specializations of parametric::serialize are defined before the first use. Inclusion order
 * matters with template specialization.
 */

#include <reflect/reflect.hpp>
#include <yaml-cpp/yaml.h>

namespace parametric {

template <typename T>
std::string serialize(T const&);

/**
 * @brief template specialization of parametric::serialize for int
 *
 * With this, root parameters of this type can be serialized to yaml
 *
 * @tparam  empty -> this is a full template specialization
 * @param v The value to be serialized
 * @return std::string string representation of the yaml node
 */
template <>
inline std::string serialize(int const& v) {
    return YAML::Node(v).as<std::string>();
}

/**
 * @brief template specialization of parametric::serialize for double
 *
 * With this, root parameters of this type can be serialized to yaml
 *
 * @tparam  empty -> this is a full template specialization
 * @param v The value to be serialized
 * @return std::string string representation of the yaml node
 */
template <>
inline std::string serialize(double const& v) {
    return YAML::Node(v).as<std::string>();
}

/**
 * @brief template specialization of parametric::serialize for std::string
 *
 * With this, root parameters of this type can be serialized to yaml
 *
 * @tparam  empty -> this is a full template specialization
 * @param v The value to be serialized
 * @return std::string string representation of the yaml node
 */
template <>
inline std::string serialize(std::string const& v) {
    return v;
}

/**
 * @brief template specialization of parametric::serialize for DynamicObject
 *
 * With this, root parameters of this type can be serialized to yaml
 *
 * @tparam  empty -> this is a full template specialization
 * @param v The value to be serialized
 * @return std::string string representation of the yaml node
 */
template <>
inline std::string serialize(reflect::DynamicObject const& v)
{
    auto serialized =
        reflect::cast<YAML::Node>(v.invoke("serialize")[0]);

    YAML::Node out = serialized;
    YAML::Emitter e;

    auto tag = YAML::VerbatimTag(v.get_type_descriptor()->get_name());
    e << tag << out;
    return e.c_str();
}

} // namespace parametric

#include <parametric/core.hpp>
