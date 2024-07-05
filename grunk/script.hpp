#pragma once 

#include <grunk/compute_nodes/Script.hpp>

namespace grunk {

/**
 * @brief Given a sequence of steps, each representing a function call of a dynamic function, 
 * as well as a list of output ids of intermediate variables, this function creates a compute node
 * that represents the evaluation of these steps within a compute node of a parametric tree. 
 *
 * This is useful, if
 *  * some operations should be performed without intermediate lazy evaluation and caching
 *  * we want to create a class and modify it using non-const setter methods.
 *
 *
 * @param steps 
 * @param returns 
 * @return ResultHolder<DynamicAction> 
 */
ResultHolder<DynamicAction> script(
    std::vector<Script::Step> const& steps,
    std::vector<std::string> const& returns
);

} // namespace grunk