/**
 * @file Action.hpp
 *
 * Declaration and Definition of the Action class.
 * 
 */

#pragma once


#include <functional>
#include <grunk/parametric_core.hpp>
#include <type_traits>
#include <utility>

#include <reflect/reflect.hpp>
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
 * factory function ::grunk::action.
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
class Action : public parametric::ComputeNode<
                        Action<F, Args...>,
                        parametric::Results<std::invoke_result_t<F, Args const&...>>,
                        parametric::Arguments<Args...>
                      >
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
     * @param id The id of the output Feature
     * @param f  the function to be wrapped
     * @param args The arguments of the function wrapped in Feature instances
     */
    Action(std::string const& id, F const& f) 
     : function(f)
    {
        this->set_id(id);
    }

public:

    /**
     * @brief evaluates the function and caches the output.
     * 
     */
    void eval() const override final
    {

        constexpr size_t nresults = std::tuple_size_v<parametric::Results<ReturnType>>;

        if constexpr (nresults > 1) {
            auto ret = call(std::make_index_sequence<sizeof...(Args)>{});
            set_outputs(std::make_index_sequence<nresults>{}, ret);
        }
        else if constexpr (nresults == 1) {
            set_output<0>(call(std::make_index_sequence<sizeof...(Args)>{}));
        }
        else {
            call(std::make_index_sequence<sizeof...(Args)>{});
        }
    }

    /**
     * @brief serialize throws an exception, as currenlty only DynamicActions can be serialized to yaml.
     * @return yaml-string representing the action.
     */
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
        return function(this->template arg<I>().value()...);
    }

    template <size_t... I>
    void set_outputs(std::index_sequence<I...>, ReturnType const& ret) const
    {
        (set_output<I>(std::get<I>(ret)), ...);
    }

    template <size_t I, typename T>
    void set_output(T const& t) const
    {
        if (auto r =  this->template res<I>(); r) {
            r->set_value(t);
        }
    }

    F const function;

};

template <typename C>
class ResultHolder
{
    using result_type = typename parametric::compute_return_value<C>;
public:

    ResultHolder(result_type const& res, C const& c) : result(res), m_compute_node(c) {}


    template <int i=0>
    decltype(auto) output() {
        if constexpr ( reflect::details::is_tuple_v<result_type> ) {
            return Feature(std::get<i>(result));
        } else {
            return Feature(result);
        }
    }

    constexpr size_t size() const {
        if constexpr (reflect::details::is_tuple_v<result_type> ) {
            return std::tuple_size_v<result_type>;
        } else {
            return 0;
        }
    }

    C const& compute_node() const {
        return m_compute_node;
    }

    void eval() const {
        compute_node().eval();
    }

private:
    C const& m_compute_node;
    result_type result;
};

namespace details {


/**
 * @brief The ActionFactory struct is an internal factory for creating Action
 * instances.
 *
 * It is a proxy class used in the free factory functions action. Factory functions are
 * needed, because Actions should always be wrapped in a parametric::compute_node_ptr
 * and the private constructor of ALgorithm makes sure that there is no misuse. The
 * factory function parametric::new_node does not work with the templated constructors of the
 * Action class, so we need new factory functions.
 *
 * The proxy factory is needed, because the factory functions action must be templated, and
 * templated friend functions are a pain in the ass. This way we have a non-templated friend
 * struct with templated member functions.
 */
struct ActionFactory
{


    /**
     * @brief TODO
     * 
     * @tparam F The type of the wrapped function
     * @tparam Args The types of the arguments expected by the wrapped function
     * @param id The id of the output of the new Action
     * @param fun The function to be wrapped in an Action instance
     * @param args The arguments wrapped in Features to be passed to the function on evaluation
     * @return TODO
     */
    template <typename F,
              typename... Args>
    static decltype(auto) new_action(std::string const& id, F const& fun, Feature<Args> const&... args)
    {
        using MyAction = Action<F, Args...>;
        auto ptr = std::shared_ptr<MyAction>(new MyAction(id, fun));
        auto ret = parametric::compute(ptr, args.param()...);
        return ResultHolder<MyAction>(ret, *ptr);
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
 * @param id The id used for the output of the function
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ActionPtr<F, Args...> A special pointer type wrapping an Action instance.
 */
template <typename F,
          typename = std::enable_if_t<
            !std::is_convertible_v<std::decay_t<F>, std::string>
            && !details::is_dynamic_callable_v<std::decay_t<F>>
          >,
          typename... Args>
decltype(auto) action(std::string const& id, F const& fun, Feature<Args> const&... args)
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
 * @param id the id of the output Feature
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ActionPtr<F, Args...> A special pointer type wrapping an Action instance.
 *
 * @ingroup static
 */
template <typename F,
          typename,
          typename... Args>
decltype(auto) action(std::string const& id, F const& fun, Args&&... args)
{
    auto to_feature = [](auto&& arg){
        using Arg = std::decay_t<decltype(arg)>;
        if constexpr (details::is_feature_v<Arg>){
            return arg;
        } else {
            return Feature("", std::forward<Arg>(arg)); //TODO: Until we properly support unnamed features, this will be an empty string
        }
    };
    return details::action(id, fun, to_feature(args)...);
}

} //namespace grunk
