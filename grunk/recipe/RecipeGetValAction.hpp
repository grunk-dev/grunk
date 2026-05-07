// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/core/parametric_core.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/recipe/Recipe.hpp"

namespace grunk {

/**
 * @brief An action to retrieve the value of a called subrecipe. This class 
 * is handled internally.
 *
 * @ingroup advanced_recipe
 */
class RecipeGetValAction : public parametric::ComputeNode<RecipeGetValAction>
{
    friend class RecipeCaller;

private:
    /**
     * @brief Access the result feature for this get-value action.
     *
     * @return A forwarding reference to the result feature.
     */
    decltype(auto) result() const;

    /**
     * @brief Access the argument/key whose value will be extracted.
     *
     * @return A forwarding reference to the argument feature.
     */
    decltype(auto) argument() const;

public:

    /**
     * @brief Construct a get-value action for recipes.
     *
     * @param name Human-readable name for the action.
     */
    RecipeGetValAction(std::string const& name);

    /**
     * @brief Connect the source recipe and the key to extract.
     *
     * @param recipe The source recipe feature.
     * @param key The name of the feature/argument to read from the recipe.
     */
    void connect_inputs(Feature<Recipe> const& recipe, std::string const& key);

    /**
     * @brief Prepare a DynamicFeature to hold the extracted value.
     *
     * @return A DynamicFeature that will receive the value.
     */
    DynamicFeature initialize_results() const;

    /**
     * @brief Attach an externally provided result DynamicFeature.
     *
     * @param res The DynamicFeature to write the extracted value into.
     */
    void connect_results(DynamicFeature const& res);

    /**
     * @brief Evaluate the action and extract the requested value.
     */
    void eval() const override;

    /**
     * @brief Serialize the action to a string representation.
     *
     * @return A serialized string describing the action.
     */
    std::string serialize() const override final;

private:
    /**
     * @brief Name used for identification or serialization.
     */
    std::string name;

    /**
     * @brief The source feature/key whose value will be retrieved.
     */
    std::string source_feature;
};

} // namespace 