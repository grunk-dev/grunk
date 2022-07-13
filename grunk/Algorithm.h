/**
 * @file Algorithm.h
 */

#pragma once


#include <functional>

#include <parametric/core.hpp>
#include <utility>

#include "Feature.h"

namespace grunk {

namespace details {

//forward declaration
struct AlgorithmFactory;

} // namespace details

template <typename F, typename... Args>
class Algorithm : public parametric::ComputeNode
{
public:
    
    static_assert(std::is_invocable_v<F, Args const& ...>, "\n\nFunction is not invocable with const references. "
        "Algorithms can only be used with referentially transparent functions.\n\n");
        
    using ReturnType = std::invoke_result_t<F, Args const&...>;

    friend struct details::AlgorithmFactory;

 private:

    Algorithm(F const& f, Feature<Args> const&... args) 
     : function(f)
     , in{std::make_tuple(args...)}
    {
        std::apply([=](Feature<Args> const&... feature){ (...,depends_on(feature.param)); }, in);
        computes(out, parametric::param<ReturnType>(""));
    }

public:

    void eval() const override final
    {
        if (!out.expired()) {
            out.set_value(call(std::make_index_sequence<sizeof...(Args)>{}));
        }
    }

    template <size_t Idx=0>
    decltype(auto) get() const
    {
        if constexpr ( !details::is_tuple_v<ReturnType> ) {
            static_assert(Idx == 0, "get with Index>0 only allowed for Algorithms returning a tuple.");
            return Feature<ReturnType>(out);
        }
        else {
            return grunk::eval([](ReturnType const& tuple){ return std::get<Idx>(tuple); }, Feature<ReturnType>(out))->get();
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

template<typename F, typename... Args>
using AlgorithmPtr = parametric::compute_node_ptr<Algorithm<F, Args...>>;

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
struct AlgorithmFactory
{

    template <typename F,
              typename... Args>
    static AlgorithmPtr<F, Args...> new_algorithm(F const& fun, Feature<Args> const&... args)
    {
        return AlgorithmPtr<F, Args...>(new Algorithm<F, Args...>(fun, args...));
    }

};

} //namespace details

template <typename F,
          typename, // default-value (enable_if) declared in Feature.h
          typename... Args>
AlgorithmPtr<F, Args...> eval(F const& fun, Feature<Args> const&... args)
{
    return details::AlgorithmFactory::new_algorithm(fun, args...);
}

} //namespace grunk
