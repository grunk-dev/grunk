// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/core/parametric_core.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/recipe/Recipe.hpp"

namespace grunk {

/**
 * @brief An action node that operates on a Recipe.
 *
 * RecipeAction is a compute node used by the recipe execution subsystem.
 * It provides facilities to connect a recipe and dynamic input features,
 * initialize and connect result features, and to evaluate the action.
 * The node can also serialize itself to a string representation.
 *
 * @ingroup advanced_recipe
 */
class RecipeAction : public parametric::ComputeNode<RecipeAction>
{
    friend class RecipeCaller;

private:

    /**
     * @brief Retrieve the result feature of this action.
     *
     * This accessor returns the internally held result feature (by
     * forwarding reference) and is intended for use by the recipe
     * caller/dispatcher.
     *
     * @return A forwarding reference to the result feature.
     */
    decltype(auto) result() const;

    /**
     * @brief Access the connected input recipe feature.
     *
     * Returns the feature that represents the recipe input connected to
     * this action.
     *
     * @return A forwarding reference to the input recipe feature.
     */
    decltype(auto) input_recipe() const;

    /**
     * @brief Access a connected input feature by index.
     *
     * @param i Index of the requested input feature.
     * @return A forwarding reference to the requested input feature.
     */
    decltype(auto) input_feature(int i) const;

public:

    /**
     * @brief Construct a RecipeAction.
     *
     * @param name Human-readable name for this action. Used for
     *              identification and serialization.
     */
    RecipeAction(std::string const& name);

    /**
     * @brief Connect inputs required by the action.
     *
     * connect_inputs binds the provided recipe feature and a mapping of
     * named dynamic features to this action's expected inputs.
     *
     * @param recipe The recipe feature to connect as the primary input.
     * @param input_map A map of feature name -> DynamicFeature to bind as
     *                  additional inputs.
     */
    void connect_inputs(Feature<Recipe> const& recipe, std::unordered_map<std::string, DynamicFeature> const& input_map);

    /**
     * @brief Prepare and return a Feature<Recipe> to hold results.
     *
     * initialize_results allocates or configures a result feature suitable
     * for storing the outputs produced by this action.
     *
     * @return A Feature<Recipe> that will hold the action results.
     */
    Feature<Recipe> initialize_results() const;

    /**
     * @brief Connect the result feature to this action.
     *
     * @param res The result feature to attach to this action node.
     */
    void connect_results(Feature<Recipe> const& res);

    /**
     * @brief Evaluate the action.
     *
     * This method performs the action's computation. It overrides the
     * ComputeNode::eval() contract.
     */
    void eval() const override;

    /**
     * @brief Serialize the action to a string.
     *
     * @return A string containing the serialized representation.
     */
    std::string serialize() const override final;

private:
    /**
     * @brief Human readable name of the action.
     */
    std::string name;

    /**
     * @brief Names of features targeted by this action.
     */
    std::vector<std::string> target_features;
};

} // namespace