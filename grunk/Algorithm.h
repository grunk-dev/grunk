/**
 * @file Algorithm.h
 */

#pragma once


#include <functional>

#include <parametric/core.hpp>
#include <utility>

#include "Feature.h"
#include "RuntimeFunction.h"

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

} // namespace details


/**
 * @brief This class does ...
 *
 * In particular ...
 */
class Algorithm : public parametric::ComputeNode
{
public:
    using InputsVec = std::vector<std::reference_wrapper<RuntimeObject const>>;
    using OutputsVec = std::vector<RuntimeObject>;
    using Function = std::function<OutputsVec(InputsVec const&)>;

    /**
        * @brief Creates a ...
        *
        * Further information ...
        */
    template <typename F>
    Algorithm(RuntimeFunction<F> const& fun, std::initializer_list<Feature> const& in)
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

    /**
        * @brief This function does ...
        *
        * Further information ...
        */
    void eval() const override;

    /**
        * @brief This function does ...
        *
        * Further information ...
        */
    Feature get(size_t idx  = 0) const;

private:
    Function function;
    std::vector<Feature> const inputs;
    std::vector<parametric::OutputParam<RuntimeObject>> mutable outputs;
};

// parametric::new_node does not work with templated ctor of Algorithm
template <typename F>
parametric::compute_node_ptr<Algorithm> new_algorithm(RuntimeFunction<F> const& fun, std::initializer_list<Feature> const& in)
{
    return parametric::compute_node_ptr<Algorithm>(new Algorithm(fun, in));
}

} //namespace grunk
