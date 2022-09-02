/**
 * @file RuntimeAlgorithm.h
 * 
 * This file implements the template specialization of Algorithm for RuntimeFunctions
 */

#pragma once

#include <vector>
#include <iterator>

#include <json/json.h>

#include <grunk/core/Algorithm.h>
#include <grunk/dynamic/RuntimeFeature.h>

namespace grunk {

namespace details {

    //forward declaration
    struct RuntimeAlgorithmFactory;

} // namespace details

/**
 * @brief template specialization of Algoithm for RuntimeFunctions
 *
 * Given a function and a set of RuntimeFeature instances as inputs, a
 * RuntimeAlgorithm represents the calculation of the function from the 
 * arguments wrapped in the input RuntimeFeature instances.
 *
 * The class has a private constructor, as it should always be created using the 
 * factory function ::grunk::eval.
 *
 * If the wrapped function returns an std::tuple, each element of this tuple
 * is interpreted as an output of the function and each element can be retrieved
 * individually as a feature.
 * 
 * @ingroup dynamic_advanced
 */
template <>
class Algorithm<Reflect::DynamicFunction> : public parametric::ComputeNode
{

    friend struct details::RuntimeAlgorithmFactory;

private:

    /**
     * @brief Construct a new RuntimeAlgorithm given a RuntimeFunction<F> and 
     * an std::initializer_list of RuntimeFeatures.
     * 
     * @param fun A const pointer to a Reflect::Function
     * @param in The input RuntimeFeatures
     */
<<<<<<< HEAD
    Algorithm(Reflect::DynamicFunction const& fun, std::initializer_list<RuntimeFeature> const& in)
     : function(fun)
=======
    template <typename F>
    Algorithm(std::string const& id, RuntimeFunction<F> const& fun, std::initializer_list<RuntimeFeature> const& in)
     : function(details::RuntimeFunctionWrapper<F>(fun))
>>>>>>> 9307593 (more file i/o experimentin)
     , inputs{in}
     , outputs(function.NumOutputs())
    {
        set_id(id);
        for (auto& i: inputs){
            depends_on(i.param());
        }
        constexpr size_t n_outputs = RuntimeFunction<F>::numOutputs;

        for (size_t i=0; i<n_outputs; ++i){
            std::string output_id = id;
            if (n_outputs > 1)             {
                output_id += "::" + std::to_string(i);
            } 
            computes(outputs[i], parametric::param<Reflect::DynamicObject>(output_id));
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
        // tranform input nodes to vector of runtime objects
        std::vector<Reflect::DynamicObject> inputs_vec;
        std::transform(inputs.begin(),
                    inputs.end(),
                    std::back_inserter(inputs_vec),
<<<<<<< HEAD
                    [](auto const& in_feature) { return in_feature.param.value(); }
=======
                    [](auto const& in_feature) { return std::cref(in_feature.param().value()); }
>>>>>>> 9307593 (more file i/o experimentin)
        );

        // call the wrapped function
        auto outputs_vals = function.Invoke(inputs_vec);

        assert(outputs_vals.size() == outputs.size());
        
        // move the output values to the output nodes
        for (size_t i=0; i < outputs.size(); ++i) {
            if (!outputs[i].expired()) {
                outputs[i].set_value(outputs_vals[i].copy());
            }
        }
    }

    /**
     * @brief returns the output(s) of the function wrapped in RuntimeFeature instances.
     *
     * If the wrapped function returns an std::tuple, each element in this 
     * tuple is interpreted as an individual output of this algorithm. This function
     * accepts a template integer argument to specify the index of the output.
     *
     * If the wrapped function returns something other than an std::tuple, 
     * there will be just one output.
     * 
     * @tparam Idx The index of the output. Defaults to zero.
     * @return decltype(auto) a Feature wrapping the output of index Idx
     */
    template <size_t Idx = 0>
    Feature<Reflect::DynamicObject> get() const
    {
        return Feature<Reflect::DynamicObject>(outputs[Idx]);
    }

    std::string serialize() const override final
    {
        //TODO: 
        // - This function should call a private function that creates
        //   an instance of json::value and convert it to string
        // - This class should know about the registered name of the function
        //   and throw an error, if the function is not registered

        Json::Value j;
        j["function"] = "function_name (To Do!)";
        for (auto const& input : inputs){
            j["inputs"].append(input.param().id());
        }
        for (auto const& output : outputs){
            j["outputs"].append(output.param().id());
        }
        Json::FastWriter writer;
        return writer.write(j);
    }

private:
    Reflect::DynamicFunction const& function;
    std::vector<RuntimeFeature> const inputs;
    std::vector<parametric::OutputParam<Reflect::DynamicObject>> mutable outputs;
};

/**
 * @brief typedef for an Algorithm wrapping a RuntimeFunction
 * @ingroup dynamic_advanced
 */
using RuntimeAlgorithm = Algorithm<Reflect::DynamicFunction>;

/**
 * @brief A parametric::compute_node_ptr wrapping a RuntimeAlgorithm
 * @ingroup dynamic_advanced
 */
using RuntimeAlgorithmPtr = AlgorithmPtr<Reflect::DynamicFunction>;

namespace details {

/**
 * @brief The AlgorithmFactory struct is an internal factory for creating Algorithm
 * instances.
 *
 * It is a proxy class used in the free factory functions eval. Factory functions are
 * needed, because Algorithms should always be wrapped in a parametric::compute_node_ptr
 * and the private constructor of ALgorithm makes sure that there is no misuse. The
 * factory function parametric::new_node does not work with the templated constructors of the
 * Algorithm class, so we need new factory functions.
 *
 * The proxy factory is needed, because the factory functions eval must be templated, and
 * templated friend functions are a pain in the ass. This way we have a non-templated friend
 * struct with templated member functions.
 */
struct RuntimeAlgorithmFactory
{
    /**
     * @brief Returns a new RuntimeAlgorithmPtr given a RuntimeFunction and an
     * initializer list of RuntimeFeatures
     * 
     * @param fun The Reflect::function to be wrapped
     * @param args The input features
     * @return RuntimeAlgorithmPtr The returned compute_node_ptr wrapping a RuntimeAlgorithm
     */
    static RuntimeAlgorithmPtr new_algorithm(
        std::string const& id, 
        Reflect::DynamicFunction const& fun, 
        std::initializer_list<Feature<Reflect::DynamicObject>> const& args
    )
    {
        return RuntimeAlgorithmPtr(new RuntimeAlgorithm(id, fun, args));;
    }

};

} //namespace details

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates a RuntimeAlgoritm instance representing the evaluation
 * of the input function for the input features.
 * 
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return RuntimeAlgorithmPtr A special pointer type wrapping a RuntimeAlgorithm instance.
 * @ingroup dynamic_advanced
 */
<<<<<<< HEAD
template <typename... Args>
RuntimeAlgorithmPtr eval(Reflect::DynamicFunction const& fun, Feature<Args> const&... args)
=======
template <typename F, typename... Args>
RuntimeAlgorithmPtr eval(std::string const& id, RuntimeFunction<F> const& fun, Feature<Args> const&... args)
>>>>>>> 9307593 (more file i/o experimentin)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(id, fun, {args...});
}

/**
 * @brief Given a function and an initializer list of features in the feature 
 * tree, this function creates a RuntimeAlgoritm instance representing the 
 * evaluation of the input function for the input features.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return RuntimeAlgorithmPtr A special pointer type wrapping a RuntimeAlgorithm instance.
 * @ingroup dynamic_advanced
 */
<<<<<<< HEAD
RuntimeAlgorithmPtr eval(Reflect::DynamicFunction const& fun, std::initializer_list<Feature<Reflect::DynamicObject>> const& args)
=======
template <typename F>
RuntimeAlgorithmPtr eval(std::string const& id, RuntimeFunction<F> const& fun, std::initializer_list<Feature<Reflect::DynamicObject>> const& args)
>>>>>>> 9307593 (more file i/o experimentin)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(id, fun, args);
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
 * @return RuntimeAlgorithmPtr A special pointer type wrapping the RuntimeAlgorithm
 *
 * @ingroup dynamic
 */
template <typename... Args>
RuntimeAlgorithmPtr eval(std::string const& name, Feature<Args> const&... args)
{
    auto const& f = Reflect::GetFunctionRegistry().Resolve(name);
    return eval(f, args...);
}

} // namespace grunk