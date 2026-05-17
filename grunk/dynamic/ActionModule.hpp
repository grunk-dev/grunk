// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/ActionDynamic.hpp"
#include "grunk/dynamic/function_meta.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "grunk/dynamic/DynamicFeature.hpp"
#include <sol/sol.hpp>
#include <vector>
#include <functional>
#include <string>

namespace grunk {

/**
 * @brief Compute node that calls a function from a named module.
 *
 * The node depends on the module source feature, so any change to the script
 * invalidates the node. At evaluation time the current function_meta is looked
 * up again, ensuring that a stale sol function is never used.
 */
class ActionModule : public parametric::ComputeNode<ActionModule>
{

    friend struct details::DynamicActionFactory;

private:

    std::string module_name_;
    std::string function_name_;
    std::function<function_meta(const std::string&)> lookup_function_;
    lua_State* lua_state_;

    inline decltype(auto) result() const {
        return this->template res<object>(0);
    }

      void eval() const override {
          // Lookup function_meta (ensures fresh binding after script reload)
          function_meta meta = lookup_function_(module_name_ + "." + function_name_);

          // Gather input values directly from parents
          // First dependency is the module source string (for invalidation only), skip it
          std::vector<object> args;
          args.reserve(this->num_parents() - 1);
          for (size_t i = 1; i < this->num_parents(); ++i) {
              auto const& parent = *this->get_parents()[i];
              if (auto const* holder = dynamic_cast<parametric::impl::param_holder<object> const*>(&parent)) {
                  args.push_back(holder->value());
              } else if (auto const* dyn_feat = dynamic_cast<parametric::impl::param_holder<DynamicFeature> const*>(&parent)) {
                  args.push_back(dyn_feat->value().value());
              } else {
                  // Unknown type - shouldn't happen with current implementation
                  const std::type_info& ti = typeid(parent);
                  throw std::runtime_error("Unknown dependency type in ActionModule: " + std::string(ti.name()));
              }
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
    // construct with callbacks for lookup
    ActionModule(
        lua_State* lua,
        std::string const& mod,
        std::string const& func,
        std::function<function_meta(const std::string&)> lookup_func
    )
        : module_name_(mod), function_name_(func), lookup_function_(std::move(lookup_func)), lua_state_(lua) {}

    // Connect inputs – similar to ActionDynamic
    void connect_inputs(std::vector<DynamicFeature> const& args) {
        for (auto const& arg : args) {
            depends_on(arg);
        }
    }

    template <typename... Args>
    void connect_inputs(Feature<Args> const&... args){
        (depends_on(args), ...);
    }

    // initialize output feature
    DynamicFeature initialize_results() const
    {
        return feature<object>({});
    }

    // bind output feature
    void connect_results(DynamicFeature const& res)
    {
        computes(res);
    }
};

/**
 * @ingroup advanced_dynamic
 * @brief Specialization of the ResultHolder class template for ActionModule. 
 */
template <>
class ResultHolder<ActionModule> {
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

struct ActionModuleFactory
{
    static ResultHolder<ActionModule> new_action(
        lua_State* lua,
        std::string const& mod,
        std::string const& func,
        std::function<function_meta(const std::string&)> lookup_func,
        std::vector<DynamicFeature> const& args
    )
    {
        auto ptr = std::shared_ptr<ActionModule>(new ActionModule(lua, mod, func, lookup_func));

        return ResultHolder<ActionModule>(
            parametric::compute(ptr, args), 
            ptr
        );
    }

    template <typename... Args>
    static ResultHolder<ActionModule> new_action(
        lua_State* lua,
        std::string const& mod,
        std::string const& func,
        std::function<function_meta(const std::string&)> lookup_func,
        Feature<Args> const&... args
    )
    {
        auto ptr = std::shared_ptr<ActionModule>(new ActionModule(lua, mod, func, lookup_func));

        return ResultHolder<ActionModule>(
            parametric::compute(ptr, args...), 
            ptr
        );
    }
};

} // namespace details

} // namespace grunk
