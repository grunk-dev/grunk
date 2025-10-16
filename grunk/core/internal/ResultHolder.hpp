#pragma once

#include "grunk/core/feature.hpp"
#include "function_traits.hpp"
#include <parametric/core.hpp>
#include <tuple>
#include <memory>

namespace grunk {

namespace details {

struct ActionFactory;

} // namespace details

namespace {

} // anonymous namespace

/**
 * @ingroup advanced
 * @brief The ResultHolder class template is a proxy for holding the result of a ::grunk::Action
 * instance. The results can be either a tuple of features, a feature or a shared_ptr<DAGNode> for void functions. 
 * 
 * @tparam C A template realization of ::grunk::Action, i.e. a specific compute node in the feature tree
 */
template <typename C>
class ResultHolder
{
    using result_type = 
    typename parametric::compute_return_value<
        parametric::Results<typename C::ReturnType>
    >;

    friend struct details::ActionFactory;

private:

    ResultHolder(result_type const& res) : result(res) {}

public:
    /**
     * @brief returns the i-th output
     * 
     * @tparam i index of the queried output
     * @return decltype(auto) the i-th output feature
     */
    template <int i=0>
    decltype(auto) output() const {
        if constexpr ( details::is_tuple_v<result_type> ) {
            return Feature(std::get<i>(result));
        } else {
            return Feature(result);
        }
    }

    /**
     * @brief returns the number of outputs
     * 
     * @return constexpr size_t the number of outputs
     */
    constexpr size_t size() const {
        if constexpr (details::is_tuple_v<result_type> ) {
            return std::tuple_size_v<result_type>;
        } else {
            return 0;
        }
    }

    /**
     * @brief returns the compute node of this action
     */
    decltype(auto) compute_node() const {
        if constexpr (std::is_void_v<typename C::ReturnType>) {
            return result;
        } else {
            return output().get_param().node_pointer()->compute_node();
        }
    }


    /**
     * @brief evaluates the compute node. 
     * 
     */
    void eval() const {
        compute_node()->eval();
    }

private:
    result_type result;
};

} // namespace grunk
