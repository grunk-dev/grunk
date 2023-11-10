#pragma once 

#include <grunk/compute_nodes/Vec.hpp>
#include <grunk/compute_nodes/DynamicVec.hpp>

namespace grunk {

/**
 * @ingroup static
 * @brief Given several Feature<T> instances, this function returns a Feature<std::vector<T>>.
 * 
 * @tparam T The type of the input features
 * @tparam Ts The type of the input features. Each U in Ts must be T
 * @param id The id of the returned feature
 * @param f The first input feature
 * @param fs The remaining input features
 * @return std::enable_if_t<
 * !std::is_same_v<std::decay_t<T>, reflect::DynamicObject>,
 * Feature<std::vector<T>>
 * > 
 */
template <typename T, typename... Ts>
std::enable_if_t<
    !std::is_same_v<std::decay_t<T>, reflect::DynamicObject>,
    Feature<std::vector<T>>
>
vec(std::string const& id, Feature<T> const& f, Feature<Ts> const&... fs)
{
    static_assert((std::is_same_v<T, Ts> && ...));
    auto ret = parametric::compute<Vec<T>>(f.param(), fs.param()...);
    ret.set_id(id);
    return ret;
}

namespace {
    // needed because of bug in MSVC 2017: Can't use a fold expression in std::enable_if_t of vec definition
    template <typename... Ts>
    constexpr bool is_dynamic_feature_v = std::is_same_v<std::tuple<DynamicFeature, Ts...>, std::tuple<Ts..., DynamicFeature>>;
}

/**
 * @ingroup dynamic
 * @brief creates a DynamicFeature type-erasing an std::vector<reflect::DynamicObject> from several DynamicFeatures.
 * 
 * @tparam Args The DynamicFeatures
 * @param id id of the returned feature
 * @param f The first input feature
 * @param fs The remaining input Features
 * @return DynamicFeature
 */
template <typename... Args>
std::enable_if_t<is_dynamic_feature_v<Args...>, DynamicFeature> 
vec(std::string const& id, DynamicFeature const& f, Args const&... fs)
{
    return vec(id, std::vector{f, fs...});
}

/**
 * @ingroup advanced
 * @brief throws an exception. ::grunk::vec should be called with at least one argument.
 * 
 * @return DynamicFeature 
 */
inline DynamicFeature vec(std::string) {
    throw std::logic_error("grunk::vec must have at least one argument.");
}

} // namespace grunk