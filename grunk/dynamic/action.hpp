// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/DynamicFeature.hpp"
#include "grunk/dynamic/function_meta.hpp"
#include "grunk/dynamic/ActionModule.hpp"
#include <string>
#include <regex>

namespace grunk {

/**
 * @brief action Creates an action representing the function evaluation given
 * the passed arguments. This will internally register this computation in the
 * underlying dependency graph.
 *
 * If one of the arguments is not yet a Feature instance, an anonymous Feature
 * (i.e. a constant) will be created on the fly.
 *
 * @param function The name of the function to be evaluated
 * @param args The arguments passed to the function.
 * @return a DynamicFeature instance representing the calculation result
 */
template <typename... Args>
DynamicFeature action(std::string const& function, Args&&... args);

/**
 * @brief action Creates an action representing the function evaluation given
 * the passed arguments. This will internally register this computation in the
 * underlying dependency graph.
 *
 * If one of the arguments is not yet a Feature instance, an anonymous Feature
 * (i.e. a constant) will be created on the fly.
 * @param function  The function to be evaluated
 * @param args The arguments passed to the function.
 * @return a DynamicFeature instance representing the calculation result
 */
template <typename... Args>
DynamicFeature action(function_meta const& function, Args&&... args);

} // namespace grunk
