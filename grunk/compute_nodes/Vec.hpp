#pragma once 

#include <grunk/Feature.hpp>
#include <grunk/parametric_core.hpp>

#include <vector>

namespace grunk {

/**
 * @ingroup advanced
 * @brief A compute node that maps several Feature<T> instances to a Feature<vector<T>>.
 *
 * The class must be instantiated via the factory function ::grunk::vec.
 * 
 * @tparam T The types of the input features
 */
template <typename T>
class Vec : public parametric::ComputeNode<Vec<T>, parametric::Results<std::vector<T>>>
{
public:
    Vec() = default;

    /**
     * @brief evaluates the compute node
     * 
     */
    void eval() const override final
    {
        std::vector<T> v;
        v.reserve(this->parents.size());
        for (int i=0; i<this->parents.size(); ++i) {
            v.push_back(this->template arg<T>(i).value());
        }
        if (auto r =  this->template res<0>(); r) {
            r->set_value(std::move(v));
        }
    }
};

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

} // namespace grunk
