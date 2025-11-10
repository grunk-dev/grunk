#pragma once

#include "grunk/core/parametric_core.hpp"

namespace grunk {

template <typename Derived, typename T>
struct FeatureBase : public parametric::param<T>
{
public:
    FeatureBase(T const& v)
    : parametric::param<T>(v, "") {}

    FeatureBase() : parametric::param<T>("") {}

    explicit FeatureBase(parametric::param<T> const& p) : parametric::param<T>(p) {}

    Derived& with_id(std::string const& id) {
        this->set_id(id);
        return static_cast<Derived&>(*this);
    }

    bool operator==(Derived const& other) {
        return this->node_pointer() == other.node_pointer();
    }

    Derived clone(
        std::shared_ptr<parametric::DAGNode::ClonedNodeMap> cloned_nodes = parametric::DAGNode::new_cloned_node_map()
    ) const
    {
        auto const p = parametric::param<T>::clone(cloned_nodes);
        return Derived(p);
    }

    bool is_placeholder() const {
        // A placeholder is a root parameter that is invalid even after evaluation
        bool is_root_parameter = this->node_pointer()->num_parents() == 0;
        if (is_root_parameter) {
            parametric::DAGNode const& node = *this->node_pointer();
            node.eval(); // trigger evaluation
            return !this->is_valid();
        }
        return false;
    }

};

} // namespace grunk