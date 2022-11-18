/**
 * @file DynamicAction.h
 * 
 * This file implements the template specialization of Action for DynamicFunctions
 */

#pragma once

#include <stdexcept>
#include <initializer_list>
#include <vector>
#include <iterator>

#include <yaml-cpp/yaml.h>

#include <grunk/core/Action.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>

namespace grunk {

namespace details {

    //forward declaration
    struct DynamicActionFactory;

} // namespace details

/**
 * @brief template specialization of Algoithm for DynamicFunctions
 *
 * Given a function and a set of DynamicFeature instances as inputs, a
 * DynamicAction represents the calculation of the function from the 
 * arguments wrapped in the input DynamicFeature instances.
 *
 * The class has a private constructor, as it should always be created using the 
 * factory function ::grunk::action.
 *
 * If the wrapped function returns an std::tuple, each element of this tuple
 * is interpreted as an output of the function and each element can be retrieved
 * individually as a feature.
 * 
 * @ingroup dynamic_advanced
 */
template <>
class Action<reflect::DynamicFunction> : public parametric::ComputeNode
{

    friend struct details::DynamicActionFactory;

private:

    /**
     * @brief Construct a new DynamicAction given a DynamicFunction<F> and 
     * an std::vector of DynamicFeatures.
     * 
     * @param fun A const pointer to a reflect::Function
     * @param in The input DynamicFeatures
     */
    Action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& in)
     : function(fun)
     , inputs{in}
     , outputs(function.num_outputs())
    {
        set_id(id);
        for (auto& i: inputs){
            depends_on(i.param());
        }

        size_t n_outputs = function.num_outputs();
        for (size_t i=0; i<n_outputs; ++i){
            std::string output_id = id;
            if (n_outputs > 1) {
                output_id += "[" + std::to_string(i) + "]";
            } 
            computes(outputs[i], parametric::param<reflect::DynamicObject>(output_id));
        }
    }

public:
    /**
     * @brief This function evaluates the wrapped function and cacnes the
     * output.
     * 
     */
    void eval() const override
    {
        // tranform input nodes to vector of DynamicObjects
        std::vector<reflect::DynamicObject> inputs_vec;
        std::transform(inputs.begin(),
                    inputs.end(),
                    std::back_inserter(inputs_vec),
                    [](auto const& in_feature) { return in_feature.param().value(); }
        );

        // call the wrapped function
        auto outputs_vals = function.invoke(inputs_vec);

        assert(outputs_vals.size() == outputs.size());
        
        // move the output values to the output nodes
        for (size_t i=0; i < outputs.size(); ++i) {
            if (!outputs[i].expired()) {
                outputs[i].set_value(outputs_vals[i].copy());
            }
        }
    }

    /**
     * @brief returns the number of outputs
     * 
     * @return size_t the number of outputs
     */
    size_t number_of_outputs() const {
        return outputs.size();
    }

    /**
     * @brief returns the output(s) of the function wrapped in DynamicFeature instances.
     *
     * If the wrapped function returns an std::tuple, each element in this 
     * tuple is interpreted as an individual output of this action. This function
     * accepts a template integer argument to specify the index of the output.
     *
     * If the wrapped function returns something other than an std::tuple, 
     * there will be just one output.
     * 
     * @param Iix The index of the output. Defaults to zero.
     * @return decltype(auto) a Feature wrapping the output of index Idx
     */
    DynamicFeature output(size_t idx = 0) const
    {
        return DynamicFeature(outputs[idx]);
    }

    // for consistency with static output
    template <size_t Idx = 0>
    DynamicFeature output() const
    {
        return output(Idx);
    }

    std::string serialize() const override final
    {

        YAML::Node y;

        // outputs
        y.push_back(YAML::Node());
        for (auto const& output : outputs){
            y[0].push_back(output.param().id());
        }

        // inputs
        y.push_back(YAML::Node());
        for (auto const& input : inputs){
            y[1].push_back(input.param().id());
        }
        y.SetStyle(YAML::EmitterStyle::Flow);
        YAML::Emitter out;

        auto tag = YAML::VerbatimTag(function.get_name());
        out << tag << y;
        return out.c_str();
    }

private:
    reflect::DynamicFunction const& function;
    std::vector<DynamicFeature> const inputs;
    std::vector<parametric::OutputParam<reflect::DynamicObject>> mutable outputs;
};

/**
 * @brief typedef for an Action wrapping a DynamicFunction
 * @ingroup dynamic_advanced
 */
using DynamicAction = Action<reflect::DynamicFunction>;

/**
 * @brief A parametric::compute_node_ptr wrapping a DynamicAction
 * @ingroup dynamic_advanced
 */
using DynamicActionPtr = ActionPtr<reflect::DynamicFunction>;

namespace details {

/**
 * @brief The ActionFactory struct is an internal factory for creating Action
 * instances.
 *
 * It is a proxy class used in the free factory functions action. Factory functions are
 * needed, because Actions should always be wrapped in a parametric::compute_node_ptr
 * and the private constructor of Action makes sure that there is no misuse. The
 * factory function parametric::new_node does not work with the templated constructors of the
 * Action class, so we need new factory functions.
 *
 * The proxy factory is needed, because the factory functions action must be templated, and
 * templated friend functions are a pain in the ass. This way we have a non-templated friend
 * struct with templated member functions.
 */
struct DynamicActionFactory
{
    /**
     * @brief Returns a new DynamicActionPtr given a DynamicFunction and an
     * vector of DynamicFeatures
     * 
     * @param fun The reflect::function to be wrapped
     * @param args The input features
     * @return DynamicActionPtr The returned compute_node_ptr wrapping a DynamicAction
     */
    static DynamicActionPtr new_action(
        std::string const& id, 
        reflect::DynamicFunction const& fun, 
        std::vector<DynamicFeature> const& args
    )
    {
        return DynamicActionPtr(new DynamicAction(id, fun, args));
    }

};

} //namespace details

/**
 * @brief Given a function and a vector of features in the feature 
 * tree, this function creates a DynamicAlgoritm instance representing the 
 * evaluation of the input function for the input features.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return DynamicActionPtr A special pointer type wrapping a DynamicAction instance.
 * @ingroup dynamic_advanced
 */
DynamicActionPtr action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args);

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates a DynamicAlgoritm instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 * 
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return DynamicActionPtr A special pointer type wrapping a DynamicAction instance.
 * @ingroup dynamic_advanced
 */
template <
    typename... Args,
    typename = std::enable_if_t<!(sizeof...(Args) == 1 && (std::is_same_v<std::vector<DynamicFeature>, std::decay_t<Args>> && ...))>
>
DynamicActionPtr action(std::string const& id, reflect::DynamicFunction const& fun, Args&&... args)
{
    auto to_feature = [](auto&& arg){
        using Arg = std::decay_t<decltype(arg)>;
        if constexpr (details::is_feature_v<Arg>){
            return arg;
        } else {
            return Feature("", reflect::DynamicObject(std::forward<Arg>(arg))); //TODO: Until we properly support unnamed features, this will be an empty string
        }
    };
    return action(id, fun, std::vector<DynamicFeature>{to_feature(std::forward<Args>(args))...});
}

/**
 * @brief Given a string identifier of a function, that has previously been registered
 * in the static function registry as well as input features of the feature tree, 
 * this function represents the evaluation of the registered function when it gets 
 * passed the input features.
 * 
 * @tparam Args The types of the arguments expected by the registered function
 * @param name The string identifier of the registered function
 * @param args The input Features
 * @return DynamicActionPtr A special pointer type wrapping the DynamicAction
 *
 * @ingroup dynamic
 */
template <typename... Args>
DynamicActionPtr action(std::string const& id, std::string const& name, Feature<Args> const&... args)
{
        auto const& f = reflect::get_function_registry().resolve(name);
        return action(id, f, args...);
}

DynamicActionPtr action(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args);

} // namespace grunk