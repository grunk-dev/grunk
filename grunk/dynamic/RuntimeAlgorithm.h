/**
 * @file RuntimeAlgorithm.h
 * 
 * This file implements the template specialization of Algorithm for RuntimeFunctions
 */

#pragma once

#include <vector>
#include <iterator>

#include <grunk/core/Algorithm.h>
#include <grunk/dynamic/RuntimeFunction.h>
#include <grunk/dynamic/RuntimeFeature.h>

namespace grunk {

namespace details {

    /**
     * @brief This is a helper class that let's us evaluate a RuntimeFunction
     * given a std::vector<RuntimeObject> as argument, rather than providing 
     * all RuntimeObjects individually as argument.
     * 
     * @tparam F The type of the wrapped function
     */
    template <typename F>
    class RuntimeFunctionWrapper 
    {
    public:

        using InputsVec = std::vector<std::reference_wrapper<RuntimeObject const>>;
        using OutputsVec = std::vector<RuntimeObject>;
        
        /**
         * @brief Construct a new RuntimeFunctionWrapper object from any (non-mutable) function
         * 
         * @param f The function to be wrapped
         */
        RuntimeFunctionWrapper(RuntimeFunction<F> const& f)
            : function(f)
        {}

        /**
         * @brief Evaluates the RuntimeFunction given an std::vector of RuntimeObjects
         * 
         * @param inputs The vector of input arguments
         * @return OutputsVec The vector of output arguments
         */
        OutputsVec operator()(InputsVec const& inputs) const
        {
            return call(std::make_index_sequence<details::function_traits<F>::arity>{}, inputs);
        }

    private:

        /**
         * @brief internal helper function to call the wrapped function
         * using the indices trick
         * 
         * @tparam Is The indices of the input arguments
         * @param inputs The vector of input arguments
         * @return OutputsVec The vector of output arguments
         */
        template<size_t... Is>
        OutputsVec call(std::index_sequence<Is...>, InputsVec const& inputs) const
        {
            return function(inputs[Is].get()...);
        }

        RuntimeFunction<F> const function;
    };

    /**
     * @brief RTAlgInputs is the input type of a RuntimeAlgorithm
     */
    using RTAlgInputs = std::vector<std::reference_wrapper<RuntimeObject const>>;

    /**
     * @brief RTAlgOuptuts is the output type of a RuntimeAlgorithm
     * 
     */
    using RTAlgOutputs = std::vector<RuntimeObject>;

    /**
     * @brief RTAlgFunction is the function wrapped by a RuntimeAlgorithm
     */
    using RTAlgFunction = std::function<RTAlgOutputs(RTAlgInputs const&)>;

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
class Algorithm<details::RTAlgFunction> : public parametric::ComputeNode
{

    friend struct details::RuntimeAlgorithmFactory;

private:

    /**
     * @brief Construct a new RuntimeAlgorithm given a RuntimeFunction<F> and 
     * an std::initializer_list of RuntimeFeatures.
     * 
     * @tparam F The type of the wrapped Function
     * @param fun Th RuntimeFunction
     * @param in The input RuntimeFeatures
     */
    template <typename F>
    Algorithm(RuntimeFunction<F> const& fun, std::initializer_list<RuntimeFeature> const& in)
     : function(details::RuntimeFunctionWrapper<F>(fun))
     , inputs{in}
     , outputs(RuntimeFunction<F>::numOutputs)
    {
        for (auto& i: inputs){
            depends_on(i.param);
        }
        for (auto& o: outputs){
            computes(o, parametric::param<RuntimeObject>(""));
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
        details::RTAlgInputs inputs_vec;
        std::transform(inputs.begin(),
                    inputs.end(),
                    std::back_inserter(inputs_vec),
                    [](auto const& in_feature) { return std::cref(in_feature.param.value()); }
        );

        // call the wrapped function
        auto outputs_vals = function(inputs_vec);

        assert(outputs_vals.size() == outputs.size());
        
        // move the output values to the output nodes
        for (size_t i=0; i < outputs.size(); ++i) {
            if (!outputs[i].expired()) {
                outputs[i].set_value(std::move(outputs_vals[i]));
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
    Feature<RuntimeObject> get() const
    {
        return Feature<RuntimeObject>(outputs[Idx]);
    }

private:
    details::RTAlgFunction function;
    std::vector<RuntimeFeature> const inputs;
    std::vector<parametric::OutputParam<RuntimeObject>> mutable outputs;
};

/**
 * @brief typedef for an Algorithm wrapping a RuntimeFunction
 * @ingroup dynamic_advanced
 */
using RuntimeAlgorithm = Algorithm<details::RTAlgFunction>;

/**
 * @brief A parametric::compute_node_ptr wrapping a RuntimeAlgorithm
 * @ingroup dynamic_advanced
 */
using RuntimeAlgorithmPtr = AlgorithmPtr<details::RTAlgFunction>;

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
     * @tparam F The type of the wrapped function
     * @param fun The RuntimeFunction<F> to be wrapped
     * @param args The input features
     * @return RuntimeAlgorithmPtr The returned compute_node_ptr wrapping a RuntimeAlgorithm
     */
    template <typename F>
    static RuntimeAlgorithmPtr new_algorithm(RuntimeFunction<F> const& fun, std::initializer_list<Feature<RuntimeObject>> const& args)
    {
        return RuntimeAlgorithmPtr(new RuntimeAlgorithm(fun, args));
    }

};

} //namespace details

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates a RuntimeAlgoritm instance representing the evaluation
 * of the input function for the input features.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return RuntimeAlgorithmPtr A special pointer type wrapping a RuntimeAlgorithm instance.
 * @ingroup dynamic_advanced
 */
template <typename F, typename... Args>
RuntimeAlgorithmPtr eval(RuntimeFunction<F> const& fun, Feature<Args> const&... args)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(fun, {args...});
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
template <typename F>
RuntimeAlgorithmPtr eval(RuntimeFunction<F> const& fun, std::initializer_list<Feature<RuntimeObject>> const& args)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(fun, args);
}

} // namespace grunk