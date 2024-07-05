#pragma once

/*
 * IMPORTANT: Always include this header rather than <parametric/core.hpp>. This makes sure that
 * the specializations of parametric::serialize are defined before the first use. Inclusion order
 * matters with template specialization.
 */

#include <grunk/common/String.hpp>
#include <reflect/reflect.hpp>
#include <yaml-cpp/yaml.h>

namespace grunk {
    namespace details {
        template <typename T>
        std::string serialize_impl(T const& v) {
            auto out = YAML::Node(v);
            YAML::Emitter e;
            auto tag = YAML::VerbatimTag(reflect::resolve<T>()->get_name());
            e << tag << out;
            return e.c_str();
        }
    }
}

namespace parametric {

template <typename T>
std::string serialize(T const&);

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
    YAML::Node out(v);

    YAML::Emitter e;

    auto tag = YAML::VerbatimTag(reflect::resolve<grunk::helper::String>()->get_name());
    e << tag << out;
    return e.c_str();
}

template <>
inline std::string serialize(grunk::helper::String const& v) {
    return serialize(std::string(v));
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

namespace grunk {

struct Serializer 
{
    template <typename T>
    static std::string serialize(T const& t) {
        if constexpr (std::is_same_v<T, reflect::DynamicObject>) {
            return parametric::serialize(t);
        } else {
            return parametric::serialize(reflect::DynamicObject(t));
        }
    }
};

template <typename T>
using param = parametric::param<T, Serializer>;

template <typename T, typename... Args>
param<T> new_param(Args&&... args) {
    return parametric::new_param<T, Serializer>(std::forward<Args>(args)...);
}

template <typename... Ts>
using Results = typename parametric::ComputeNodeTraits<Serializer>::template Results<Ts...>;

template <typename... Ts>
using Arguments = typename parametric::ComputeNodeTraits<Serializer>::template Arguments<Ts...>;

} // namespace grunk