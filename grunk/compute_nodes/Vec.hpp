#pragma once 

#include <grunk/Feature.hpp>
#include <grunk/common/parametric_core.hpp>

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
class Vec : public parametric::ComputeNode<Vec<T>, Results<std::vector<T>>>
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
            v.push_back(this->template arg<T, Serializer>(i).value());
        }
        if (auto r =  this->template res<0>(); r) {
            r->set_value(std::move(v));
        }
    }
};

} // namespace grunk
