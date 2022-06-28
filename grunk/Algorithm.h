/**
 * @file Algorithm.h
 */

#pragma once


#include <functional>
#include <vector>
#include <iterator>

#include <parametric/core.hpp>
#include <utility>

#include "Feature.h"
#include "RuntimeFunction.h"

namespace grunk {

template <typename F, typename... Args>
class Algorithm : public parametric::ComputeNode
{
public:
    using ReturnType = std::invoke_result_t<F, Args...>;

    Algorithm(F const& f, Feature<Args> const&... args) 
     : in(std::make_tuple(args...))
     , function(f)
    {
        std::apply([=](auto&... feature){ (...,depends_on(feature.param)); }, in);
        computes(out, parametric::param<ReturnType>(""));
    }

    void eval() const override
    {
        if (!out.expired()) {
            out.set_value(call(std::make_index_sequence<sizeof...(Args)>{}));
        }
    }

    template <size_t Idx=0>
    decltype(auto) get() const
    {

        if constexpr ( details::is_tuple_v<ReturnType> ) {
            return Feature<std::tuple_element_t<Idx, ReturnType>>(std::get<Idx>(out));
        }
        else {
            static_assert(Idx == 0, "get with Index>0 only allowed for Algorithms returning a tuple.");
            return Feature<ReturnType>(out);
        }
    }

private:
    template <size_t... I>
    ReturnType call(std::index_sequence<I...>) const
    {
        return function(std::get<I>(in).value()...);
    }

    F const function;
    std::tuple<Feature<Args> const...> const in;
    parametric::OutputParam<ReturnType> mutable out;

};

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

} // namespace details


/**
 * @brief This class does ...
 *
 * In particular ...
 */
template <>
class Algorithm<details::RTAlgFunction> : public parametric::ComputeNode
{
public:

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
        for (int i=0; i<outputs.size(); ++i) {
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

template <typename F, typename... Args>
parametric::compute_node_ptr<Algorithm<F, Args...>> algorithm(F const& fun, Feature<Args> const&... args)
{
    // parametric::new_node does not work with templated ctor of Algorithm
    return parametric::compute_node_ptr<Algorithm<F, Args...>>(new Algorithm<F, Args...>(fun, args...));
}

template <typename F, typename... Args>
parametric::compute_node_ptr<RuntimeAlgorithm> algorithm(RuntimeFunction<F> const& fun, Feature<Args> const&... args)
{
    // parametric::new_node does not work with templated ctor of Algorithm
    return parametric::compute_node_ptr<RuntimeAlgorithm>(new RuntimeAlgorithm(fun, {args...}));
}

template <typename F>
parametric::compute_node_ptr<RuntimeAlgorithm> algorithm(RuntimeFunction<F> const& fun, std::initializer_list<Feature<RuntimeObject>> const& args)
{
    // parametric::new_node does not work with templated ctor of Algorithm
    return parametric::compute_node_ptr<RuntimeAlgorithm>(new RuntimeAlgorithm(fun, args));
}

} //namespace grunk
