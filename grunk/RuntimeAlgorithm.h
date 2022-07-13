/**
 * @file RuntimeAlgorithm.h
 */

#pragma once

#include "Algorithm.h"

#include <vector>
#include <iterator>

#include "RuntimeFunction.h"
#include "RuntimeFeature.h"

namespace grunk {

namespace details {

    template <typename F>
    class RuntimeFunctionWrapper 
    {
    public:

        using InputsVec = std::vector<std::reference_wrapper<RuntimeObject const>>;
        using OutputsVec = std::vector<RuntimeObject>;
        
        RuntimeFunctionWrapper(RuntimeFunction<F> const& f)
            : function(f)
        {}

        OutputsVec operator()(InputsVec const& inputs) const
        {
            return call(std::make_index_sequence<details::function_traits<F>::arity>{}, inputs);
        }

    private:

        template<size_t... Is>
        OutputsVec call(std::index_sequence<Is...>, InputsVec const& inputs) const
        {
            return function(inputs[Is].get()...);
        }

        RuntimeFunction<F> const function;
    };

    using RTAlgInputs = std::vector<std::reference_wrapper<RuntimeObject const>>;
    using RTAlgOutputs = std::vector<RuntimeObject>;
    using RTAlgFunction = std::function<RTAlgOutputs(RTAlgInputs const&)>;

    //forward declaration
    struct RuntimeAlgorithmFactory;

} // namespace details

template <>
class Algorithm<details::RTAlgFunction> : public parametric::ComputeNode
{

    friend struct details::RuntimeAlgorithmFactory;

private:

    /**
        * @brief Creates a ...
        *
        * Further information ...
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
        * @brief This function does ...
        *
        * Further information ...
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
        * @brief This function does ...
        *
        * Further information ...
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

using RuntimeAlgorithm = Algorithm<details::RTAlgFunction>;
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

    template <typename F>
    static RuntimeAlgorithmPtr new_algorithm(RuntimeFunction<F> const& fun, std::initializer_list<Feature<RuntimeObject>> const& args)
    {
        return RuntimeAlgorithmPtr(new RuntimeAlgorithm(fun, args));
    }

};

} //namespace details

template <typename F, typename... Args>
RuntimeAlgorithmPtr eval(RuntimeFunction<F> const& fun, Feature<Args> const&... args)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(fun, {args...});
}

template <typename F>
RuntimeAlgorithmPtr eval(RuntimeFunction<F> const& fun, std::initializer_list<Feature<RuntimeObject>> const& args)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(fun, args);
}

} // namespace grunk