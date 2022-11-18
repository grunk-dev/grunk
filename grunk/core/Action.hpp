/**
 * @file Action.h
 *
 * Declaration and Definition of the Action class.
 * 
 */

#pragma once


#include <functional>

#include <parametric/core.hpp>
#include <utility>

#include "Feature.hpp"

namespace grunk {

namespace details {

//forward declaration
struct ActionFactory;

} // namespace details

/**
 * @ingroup advanced
 * @brief The Action class is a compute node in the feature tree of grunk.
 * 
 * Given a function and a set of Feature instances as inputs, an Action represents
 * the calculation of the function from the arguments wrapped in the input Feature 
 * instances.
 *
 * The class has a private constructor, as it should always be created using the 
 * factory function ::grunk::eval.
 *
 * If the wrapped function returns an std::tuple, each element of this tuple
 * is interpreted as an output of the function and each element can be retrieved
 * individually as a feature.
 * 
 * @tparam F     The type of the function to be wrapped. This can be any referentially transparent function, 
                 In particular, the function must be invokable on const 
                 references.
 * @tparam Args  The type of the arguments, the wrapped function expects.
 */
template <typename F, typename... Args>
class Action : public parametric::ComputeNode
{
public:
    
    static_assert(std::is_invocable_v<F, Args const& ...>, "\n\nFunction is not invocable with const references. "
        "Actions can only be used with referentially transparent functions.\n\n");
        
    //TODO: This produces a false warning and a false template<> annotation
    //I think this is related to https://github.com/michaeljones/breathe/issues/407, 
    //which was fixed in a more recent breathe/sphinx version than the one we ware currently
    //using

    /**
     * @brief The return type of the wrapped function
     */
    using ReturnType = std::invoke_result_t<F, Args const&...>;

    friend struct details::ActionFactory;

 private:

    /**
     * @brief Construct a new Action object
     * 
     * @param f  the function to be wrapped
     * @param args The arguments of the function wrapped in Feature instances
     */
    Action(std::string const& id, F const& f, Feature<Args> const&... args) 
     : function(f)
     , in{std::make_tuple(args...)}
    {
        std::apply([=](Feature<Args> const&... feature){ (...,depends_on(feature.param())); }, in);
        computes(out, parametric::param<ReturnType>(id));
        set_id(id);
    }

public:

    /**
     * @brief evaluates the function and caches the output.
     * 
     */
    void eval() const override final
    {
        if (!out.expired()) {
            out.set_value(call(std::make_index_sequence<sizeof...(Args)>{}));
        }
    }

    /**
     * @brief returns the output(s) of the function wrapped in Feature instances.
     *
     * If the wrapped function returns an std::tuple, each element in this 
     * tuple is interpreted as an individual output of this action. This function
     * accepts a template integer argument to specify the index of the output.
     *
     * If the wrapped function returns something other than an std::tuple, 
     * there will be just one output.
     * 
     * @tparam Idx The index of the output. Defaults to zero.
     * @return decltype(auto) a Feature wrapping the output of index Idx
     */
    template <size_t Idx=0>
    decltype(auto) get() const
    {
        if constexpr ( !reflect::details::is_tuple_v<ReturnType> ) {

            if constexpr ( !std::is_same_v<std::vector<reflect::DynamicObject>, std::decay_t<ReturnType>>) {
                static_assert(Idx == 0, "get with Index>0 only allowed for Actions returning a tuple.");
                return Feature<ReturnType>(out);
            } else {
                return grunk::eval(
                    out.param().id() + "[" + std::to_string(Idx) + "]",
                    [](ReturnType const& vec){ return vec[Idx]; }, 
                    Feature<ReturnType>(out)
                )->get();
            }
        }
        else {
            //TODO: Why do we need to "eval" this again? Isn't this overkill a bit?
            return grunk::eval(
                out.param().id() + "[" + std::to_string(Idx) + "]",
                [](ReturnType const& tuple){ return std::get<Idx>(tuple); }, 
                Feature<ReturnType>(out)
            )->get();
        }
    }

    std::string serialize() const override final
    {
        throw std::logic_error("Only Actions wrapping a registered dynamic function can be serialized\n");
    }

private:

    /**
     * @brief internal helper function to query the ith index of the input
     * arguments using the index trick
     * 
     * @tparam I indices of the input arguments
     * @return ReturnType the retun value of the wrapped function
     */
    template <size_t... I>
    ReturnType call(std::index_sequence<I...>) const
    {
        return function(std::get<I>(in).value()...);
    }

    F const function;
    std::tuple<Feature<Args> const...> const in;
    parametric::OutputParam<ReturnType> mutable out;

};

/**
 * @brief A parametric::compute_node_ptr wrapping an Action instance
 * 
 * @tparam F the type of the function wrapped in the action instance
 * @tparam Args The types of the arguments expected by the wrapped function
 */
template<typename F, typename... Args>
using ActionPtr = parametric::compute_node_ptr<Action<F, Args...>>;

namespace details {

/**
 * @brief The ActionFactory struct is an internal factory for creating Action
 * instances.
 *
 * It is a proxy class used in the free factory functions eval. Factory functions are
 * needed, because Actions should always be wrapped in a parametric::compute_node_ptr
 * and the private constructor of Action makes sure that there is no misuse. The
 * factory function parametric::new_node does not work with the templated constructors of the
 * Action class, so we need new factory functions.
 *
 * The proxy factory is needed, because the factory functions eval must be templated, and
 * templated friend functions are a pain in the ass. This way we have a non-templated friend
 * struct with templated member functions.
 */
struct ActionFactory
{

    /**
     * @brief returns an ActionPtr
     * 
     * @tparam F The type of the wrapped function
     * @tparam Args The types of the arguments expected by the wrapped function
     * @param fun The function to be wrapped in an Action instance
     * @param args The arguments wrapped in Features to be passed to the function on evaluation
     * @return ActionPtr<F, Args...> a parametric::compute_node_ptr wrapping the Action instance
     */
    template <typename F,
              typename... Args>
    static ActionPtr<F, Args...> new_action(std::string const& id, F const& fun, Feature<Args> const&... args)
    {
        return ActionPtr<F, Args...>(new Action<F, Args...>(id, fun, args...));
    }

};

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates an Action instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts only features as arguments to the given function.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ActionPtr<F, Args...> A special pointer type wrapping an Action instance.
 */
template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_dynamic_function_v<std::decay_t<F>>
          >,
          typename... Args>
ActionPtr<F, Args...> eval(std::string const& id, F const& fun, Feature<Args> const&... args)
{
    return details::ActionFactory::new_action(id, fun, args...);
}


} //namespace details

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates an Action instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @tparam Args The types of the arguments expected by the input function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ActionPtr<F, Args...> A special pointer type wrapping an Action instance.
 *
 * @ingroup static
 */
template <typename F,
          typename,
          typename... Args>
decltype(auto) eval(std::string const& id, F const& fun, Args&&... args)
{
    auto to_feature = [](auto&& arg){
        using Arg = std::decay_t<decltype(arg)>;
        if constexpr (details::is_feature_v<Arg>){
            return arg;
        } else {
            return Feature("", std::forward<Arg>(arg)); //TODO: Until we properly support unnamed features, this will be an empty string
        }
    };
    return details::eval(id, fun, to_feature(args)...);
}

} //namespace grunk
