#pragma once 

#include "DynamicFeature.hpp"
#include <string>
#include <unordered_map>

namespace grunk {

/**
 * @brief When deserializing a feature tree from yaml, the features
 * of the feature tree will be contained in a FeatureContainer.
 *
 * It is a typedef for an unordered_map, where the keys are the ids
 * of the features.
 * 
 * @ingroup fileio
 */
using FeatureContainer = std::unordered_map<std::string, DynamicFeature>;

} // namespace grunk