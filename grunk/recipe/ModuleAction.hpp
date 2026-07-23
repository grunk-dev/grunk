// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/core/parametric_core.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/dynamic/DynamicFeature.hpp"

#include <optional>

namespace grunk {

namespace details {
    struct ModuleActionFactory;
} // namespace details

/**
 * @brief A compute node representing the invocation of a single function defined in a
 * grunk module script that belongs to a Recipe.
 *
 * A ModuleAction depends on the ``Feature<std::string>`` holding the module's Lua source,
 * in addition to its regular input features. Whenever the script feature's value changes,
 * the module is recompiled the next time this action is evaluated, so calling a module
 * function behaves like any other feature-dependent computation: editing
 * ``Recipe::module_scripts`` invalidates and recomputes every ModuleAction that was built
 * from a call into that module.
 *
 * The body of a module function itself is *not* parametric: statements inside it run as
 * plain, uninstrumented Lua against concrete values. This is what allows non-const setters
 * to be used safely to build up objects, see the "Modules" section of the documentation.
 *
 * @ingroup advanced_recipe
 */
class ModuleAction : public parametric::ComputeNode<ModuleAction>
{
    friend struct details::ModuleActionFactory;

private:

    /**
     * @brief Construct a ModuleAction. Always created through details::ModuleActionFactory.
     *
     * @param module_name name of the module this action calls into
     * @param function_name name of the function within the module to call
     * @param lua the Lua state used to (re)compile the module and evaluate the function call
     */
    ModuleAction(std::string const& module_name, std::string const& function_name, lua_State* lua)
     : module_name(module_name)
     , function_name(function_name)
     , lua(lua)
    {}

    inline decltype(auto) result() const {
        return this->template res<object>(0);
    }

    inline decltype(auto) script() const {
        return this->template arg<std::string>(0);
    }

    inline decltype(auto) argument(int i) const {
        return this->template arg<object>(i);
    }

public:

    /**
     * @brief connect the inputs to this compute node
     *
     * @param script the module's source code, as a Feature<std::string>
     * @param args the arguments passed to the function call
     */
    void connect_inputs(Feature<std::string> const& script, std::vector<DynamicFeature> const& args)
    {
        depends_on(script);
        for (auto const& arg : args) {
            depends_on(arg);
        }
    }

    /**
     * @brief initializes an empty feature for the output of this action
     */
    DynamicFeature initialize_results() const
    {
        return feature<object>({});
    }

    /**
     * @brief connects this compute node with the inputs and outputs
     */
    void connect_results(DynamicFeature const& res)
    {
        computes(res);
    }

    /**
     * @brief (Re-)compiles the module if the script has changed since the last evaluation
     * and calls the requested function with the current argument values.
     */
    void eval() const override;

    /**
     * @brief serialize a ModuleAction to string
     */
    std::string serialize() const override final;

private:

    std::string module_name;
    std::string function_name;
    lua_State* lua;

    /// @brief the script that was compiled into compiled_module, used to detect changes
    mutable std::optional<std::string> cached_script;
    /// @brief a table holding the compiled functions of the module, keyed by function name
    mutable sol::table compiled_module;
};

/**
 * @ingroup advanced_recipe
 * @brief Specialization of the ResultHolder class template for ModuleActions, analogous to
 * the specialization for ActionDynamic.
 */
template <>
class ResultHolder<ModuleAction> {
    using result_type = DynamicFeature;

public:
    ResultHolder(result_type const& res, std::shared_ptr<parametric::DAGNode> const& c) : result(res), m_compute_node(c) {}

    decltype(auto) output() const {
        return result;
    }

    std::shared_ptr<parametric::DAGNode> const& compute_node() const {
        return m_compute_node;
    }

    void eval() const {
        compute_node()->eval();
    }

private:
    std::shared_ptr<parametric::DAGNode> m_compute_node;
    result_type result;
};

namespace details {

/**
 * @brief Internal factory for creating ModuleAction instances, following the same pattern as
 * details::DynamicActionFactory for ActionDynamic.
 */
struct ModuleActionFactory
{
    static ResultHolder<ModuleAction> new_action(
        std::string const& module_name,
        std::string const& function_name,
        lua_State* lua,
        Feature<std::string> const& script,
        std::vector<DynamicFeature> const& args
    )
    {
        auto ptr = std::shared_ptr<ModuleAction>(new ModuleAction(module_name, function_name, lua));

        return ResultHolder<ModuleAction>(
            parametric::compute(ptr, script, args),
            ptr
        );
    }
};

} // namespace details

} // namespace grunk
