#pragma once 

#include <grunk/common/parametric_core.hpp>
#include <grunk/FeatureBase.hpp>

#include <parametric/core.hpp>
#include <tuple>
#include <memory>

namespace grunk {

    template <typename T>
    class Feature;

namespace details {

struct ActionFactory;

} // namespace details

namespace {

/**
 * @brief A meta-programming helper struct to transform param<T> to Feature<T> and 
 * std::tuple<param<Ts>...> to std::tuple<Feature<Ts>...>
 *
 * Default is void so that it can be applied to all three possible return values of 
 * parametric::compute
 * 
 * @tparam T 
 */
template <typename T>
struct Param2Feature
{
    // handles the case where T is neither a param, nor a tuple of params
    using type = void;
};

// specialization for param<T>
template <typename T>
struct Param2Feature<param<T>>
{
    using type=Feature<T>;
};

// specialization for tuple<param<Ts>...>
template <typename... Ts>
struct Param2Feature<std::tuple<param<Ts>...>>
{
    using type = std::tuple<Feature<Ts>...>;
};

// in case parametric::compute returns a pointer to a DAGNode (void functions), the type stays the same
template <>
struct Param2Feature<std::shared_ptr<parametric::DAGNode>>
{
    using type = std::shared_ptr<parametric::DAGNode>;
};

template <typename... Ts>
using param2feature_t = typename Param2Feature<Ts...>::type;

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
    using result_type = param2feature_t<
        typename parametric::compute_return_value<Results<typename C::ReturnType>>
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
        if constexpr ( reflect::details::is_tuple_v<result_type> ) {
            return std::get<i>(result);
        } else {
            return result;
        }
    }

    /**
     * @brief returns the number of outputs
     * 
     * @return constexpr size_t the number of outputs
     */
    constexpr size_t size() const {
        if constexpr (reflect::details::is_tuple_v<result_type> ) {
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
