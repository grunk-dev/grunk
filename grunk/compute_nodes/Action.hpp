/**
 * @file Action.hpp
 *
 * Declaration and Definition of the Action class.
 * 
 */

#pragma once

#include <grunk/common/parametric_core.hpp>
#include <grunk/common/ResultHolder.hpp>
#include <grunk/Feature.hpp>

#include <reflect/reflect.hpp>

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace grunk {

namespace details {

//forward declaration
struct ActionFactory;

YAML::Node serialize(parametric::DAGNode const& node);

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
 * individually as a feature using the ::grunk::ResultHolder class template.
 * 
 * @tparam F     The type of the function to be wrapped. This can be any referentially transparent function, 
                 In particular, the function must be invokable on const 
                 references.
 * @tparam Args  The type of the arguments, the wrapped function expects.
 */
template <typename F, typename... Args>
class Action : public parametric::ComputeNode<
                        Action<F, Args...>,
                        Results<std::invoke_result_t<F, Args const&...>>,
                        Arguments<Args...>
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

    static constexpr size_t nresults = std::tuple_size_v<parametric::Results<ReturnType>>;

    /**
     * @brief evaluates the function and caches the output.
     * 
     */
    void eval() const override final
    {

        if constexpr (nresults > 1) {
            auto ret = call(std::make_index_sequence<sizeof...(Args)>{});
            this->set_outputs(std::make_index_sequence<nresults>{}, ret);
        }
        else if constexpr (nresults == 1) {
            this->set_output<0>(call(std::make_index_sequence<sizeof...(Args)>{}));
        }
        else {
            call(std::make_index_sequence<sizeof...(Args)>{});
        }
    }

    /**
     * @brief call-back to set the ids of the outputs, once this compute node is connected
     * with its parents and children in the feature tree.
     * 
     */
    void post_connect() const
    {
        set_output_ids(std::make_index_sequence<nresults>{});
    }

    /**
     * @brief serialize throws an exception, as currenlty only DynamicActions can be serialized to yaml.
     * @return yaml-string representing the action.
     */
    std::string serialize() const override final
    {
        std::string function_name;
        auto fopt = reflect::resolve_function(function, reflect::ToOptionalTag{});
        if (!fopt) {
            throw std::logic_error("Cannot serialize Action of an unregistered function.");
        } else {
            function_name = (*fopt)->get_full_name();
        }

        YAML::Node y;

        // outputs
        y.push_back(YAML::Node());
        for (auto const& child : this->get_children()){
            if (!child.expired()) {
                y[0].push_back(child.lock()->id());
            }
        }

        // inputs
        y.push_back(YAML::Node());
        for (auto const& input : this->get_parents()){
            y[1].push_back(details::serialize(*input));
        }
        y.SetStyle(YAML::EmitterStyle::Flow);
        YAML::Emitter out;

        auto tag = YAML::VerbatimTag(function_name);
        out << tag << y;
        return out.c_str();
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
        return std::invoke(function, this->template arg<I>().value()...);
    }

    /**
     * @brief internal helper function to set the outputs with the new calcuation
     * result using the index trick. Here it is assumed that the function returns 
     * a tuple.
     * 
     * @tparam I indices of the output values
     * @param ret The tuple returned by the wrapped function
     */
    template <typename T, size_t... I>
    void set_outputs(std::index_sequence<I...>, T const& ret) const
    {
        (set_output<I>(std::get<I>(ret)), ...);
    }

    /**
     * @brief internal helper function to set the output of an individual output
     * using the index trick
     * 
     * @tparam I index of the output
     * @tparam T type of the output
     * @param t new value for the output
     */
    template <size_t I, typename T>
    void set_output(T const& t) const
    {
        if (auto r =  this->template res<I>(); r) {
            r->set_value(t);
        }
    }

    /**
     * @brief internal helper function to set the output ids 
     * using the index trick. Here it is assumed that the function returns 
     * a tuple.
     * 
     * @tparam I indices of the output values
     */
    template <size_t... I>
    void set_output_ids(std::index_sequence<I...>) const
    {
        (set_output_id<I>(), ...);
    }

    /**
     * @brief internal helper function to set the output id of an individual output
     * using the index trick
     * 
     * @tparam I index of the output
     */
    template <size_t I>
    void set_output_id() const
    {
        if (auto r =  this->template res<I>(); r) {
            r->set_id(this->id());
        }
    }

    F const function;

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
     * @brief construct a new Action instance.
     * 
     * @tparam F The type of the wrapped function
     * @tparam Args The types of the arguments expected by the wrapped function
     * @param id The id of the output of the new Action
     * @param fun The function to be wrapped in an Action instance
     * @param args The arguments wrapped in Features to be passed to the function on evaluation
     * @return ResultHolder wrapping the outputs of the Action
     */
    template <typename F,
              typename... Args>
    static ResultHolder<Action<F, Args...>> new_action(std::string const& id, F const& fun, Feature<Args> const&... args)
    {
        using MyAction = Action<F, Args...>;
        return ResultHolder<MyAction>(
            parametric::compute(
                std::shared_ptr<MyAction>(new MyAction(id, fun)), 
                args.get_param()...
            )
        );
    }

};

} //namespace details

} //namespace grunk
