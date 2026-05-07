// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

/**
 * @file Feature.hpp
 *
 * Declaration and Definition of the Feature class.
 */

#pragma once

#include "grunk/core/parametric_core.hpp"

namespace grunk {

/**
 * @ingroup core
 * @brief The FeatureBase class template is a base class for the Feature class template. It provides some common functionality for all features, such as cloning, comparison and placeholder detection. It is not meant to be used directly by users, but rather to be inherited by the Feature class template.
 *
 * The class uses the Curiously Recurring Template Pattern (CRTP) to allow the base class to return the derived class type in its methods, such as with_id and clone. This allows for more convenient method chaining and cloning of features.
 *
 * @tparam Derived the derived class, i.e. the Feature class template that inherits from this base class template
 * @tparam T the type of the feature value, e.g. double, std::string or even a user defined type
 * 
 * @ingroup core
 */
template <typename Derived, typename T>
struct FeatureBase : public parametric::param<T>
{
public:

    /**
     * @brief constructs a feature with the given value. The value can be a literal, a variable or the output of an action. 
     *
     * @param v the value of the feature
     */
    FeatureBase(T const& v)
    : parametric::param<T>("", v) {}

    FeatureBase() : parametric::param<T>("") {}

    /**
     * @brief constructs a feature from a parametric::param. This is used internally for cloning and other operations that manipulate the underlying DAG.
     * 
     * @param p the parametric::param to construct the feature from
     */
    explicit FeatureBase(parametric::param<T> const& p) : parametric::param<T>(p) {}

    /**
     * @brief sets the id of the feature and returns a reference to the feature itself. This is useful for chaining method calls.
     * 
     * @param id the id to set
     * @return Derived& a reference to the feature itself
     */
    Derived& with_id(std::string const& id) {
        this->set_id(id);
        return static_cast<Derived&>(*this);
    }

    /**
     * @brief compares two features for equality. Two features are considered equal if they point to the same node in the DAG, i.e. they are the same feature in the feature tree. This is a shallow comparison that does not compare the values of the features, but rather their identity in the DAG.
     * 
     * @param other the other feature to compare with
     * @return true if the features are equal, false otherwise
     */
    bool operator==(Derived const& other) {
        return this->node_pointer() == other.node_pointer();
    }

    /**
     * @brief clones the feature and its underlying DAG node. This is a deep copy that creates a new feature with the same value and id, but a different identity in the DAG. The cloned nodes map is used to keep track of already cloned nodes and avoid infinite recursion in case of cyclic dependencies.
     * @param cloned_nodes a shared pointer to a map that keeps track of already cloned nodes. This is used internally to avoid infinite recursion when cloning features with cyclic dependencies. Users typically do not need to provide this argument, as it is handled automatically by the clone method.
     * @return Derived a new feature that is a clone of the original feature, with the same value and id, but a different identity in the DAG.
     */
    Derived clone(
        std::shared_ptr<parametric::DAGNode::ClonedNodeMap> cloned_nodes = parametric::DAGNode::new_cloned_node_map()
    ) const
    {
        auto const p = parametric::param<T>::clone(cloned_nodes);
        return Derived(p);
    }

    /**
     * @brief is_placeholder returns true if the feature is a placeholder, i.e. a root parameter that is invalid even after evaluation. This can be used to detect features that are meant to be filled in later, e.g. by a recipe call action, but have not been assigned a value yet.
     * 
     * @return true if the feature is a placeholder, false otherwise
     */
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