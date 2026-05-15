// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/ActionDynamic.hpp"
#include "grunk/dynamic/function_meta.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/dynamic/DynamicFeature.hpp"
// forward declaration to avoid circular include
namespace grunk { class state; }
#include <sol/sol.hpp>
#include <vector>

namespace grunk {

/**
 * @brief Compute node that calls a function from a named module.
 *
 * The node depends on the module source feature stored in `state::module_features_`,
 * so any change to the script invalidates the node. At evaluation time the current
 * `function_meta` is looked up again, ensuring that a stale sol function is never used.
 */
class ActionModule : public parametric::ComputeNode<ActionModule>
{
    friend struct details::DynamicActionFactory;

private:
    const state* owner_;               // non‑owning pointer to the owning state
    std::string module_name_;          // name of the module
    std::string function_name_;        // name of the function inside the module
    function_meta cached_meta_;        // cached meta for fast call after first lookup
    std::vector<DynamicFeature> inputs_;

    // evaluate the function each call – lookup ensures fresh binding
    void eval() const override {
        // Resolve (and depend on) the module source feature – this creates the dependency
        owner_->module_features_.at(module_name_); // dependency only; value not used directly

        // Re‑lookup the latest function_meta (may have been re‑created after script change)
        function_meta meta = owner_->get_function(module_name_ + "." + function_name_);

        // Gather input values
        std::vector<object> args;
        args.reserve(inputs_.size());
        for (auto const& f : inputs_) {
            args.push_back(f.value());
        }
        // Call
        sol::protected_function_result res = meta.call(sol::as_args(args));
        if (!res.valid()) {
            sol::error err = res;
            throw std::runtime_error(std::string("Error evaluating module action: ") + err.what());
        }
        // Set output
        if (auto out = result(); out) {
            out->set_value(res[0]);
        }
    }

public:
    // construct with owner and function meta (metadata not needed at ctor)
    ActionModule(const state* owner, std::string const& mod, std::string const& func)
        : owner_(owner), module_name_(mod), function_name_(func) {}

    // Connect inputs – similar to ActionDynamic
    void connect_inputs(std::vector<DynamicFeature> const& args) {
        inputs_ = args; // copy
        for (auto const& a : args) {
            depends_on(a);
            evaluators.push_back(make_evaluator(a));
        }
    }

    // initialize output feature
    DynamicFeature initialize_results() const { return feature<object>({}); }

    // bind output feature
    void connect_results(DynamicFeature const& out) { computes(out); }
};

// Factory helper placed in state (exposed later)

} // namespace grunk
