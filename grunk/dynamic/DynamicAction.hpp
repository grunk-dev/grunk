/**
 * @file DynamicAction.hpp
 * 
 * This file implements the template specialization of Action for DynamicFunctions
 */

#pragma once

#include <stdexcept>
#include <initializer_list>
#include <type_traits>
#include <vector>
#include <iterator>

#include <yaml-cpp/yaml.h>

#include <grunk/core/Action.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/helper/String.hpp>

namespace grunk {

class Recipe;

namespace details {

    //forward declaration
    struct DynamicActionFactory;

} // namespace details

/**
 * @brief template specialization of Algoithm for DynamicFunctions
 *
 * Given a function and a set of DynamicFeature instances as inputs, a
 * DynamicAction represents the calculation of the function from the 
 * arguments wrapped in the input DynamicFeature instances.
 *
 * The class has a private constructor, as it should always be created using the 
 * factory function ::grunk::action.
 *
 * If the wrapped function returns an std::tuple, each element of this tuple
 * is interpreted as an output of the function and each element can be retrieved
 * individually as a feature.
 * 
 * @ingroup dynamic_advanced
 */
template <>
class Action<reflect::DynamicFunction> : public parametric::ComputeNode<
                                                    Action<reflect::DynamicFunction>,
                                                    parametric::Results<std::vector<reflect::DynamicObject>>, /* Results are ignored*/
                                                    parametric::Arguments<std::vector<reflect::DynamicObject>> /* Arguments are ignored by derived class */
                                                >
{

    friend struct details::DynamicActionFactory;

private:

    /**
     * @brief Construct a new DynamicAction given a DynamicFunction<F> and 
     * an std::vector of DynamicFeatures.
     * 
     * @param id The id of the output Feature
     * @param fun A const pointer to a reflect::Function
     * @param in The input DynamicFeatures
     */
    Action(std::string const& id, reflect::DynamicFunction const& fun)
     : function(fun)
    {
        set_id(id);
    }

public:


    void connect_inputs(std::vector<DynamicFeature> const& args)
    {
        for (auto const& arg : args) {
            depends_on(arg.param());
        }
    }

    std::vector<DynamicFeature> initialize_results() const
    {
        std::vector<DynamicFeature> res;
        res.reserve(function.num_outputs());

        for (size_t i = 0; i < function.num_outputs(); ++i) {
            res.emplace_back(parametric::new_param<reflect::DynamicObject>(), function.get_return_type(i));
        }
        return res;
    }

    void connect_results(std::vector<DynamicFeature> const& res)
    {
        for (auto& f : res) {
            computes(f.param());
        }
    }

    void post_connect() const
    {
        for (int i = 0; i < function.num_outputs(); ++i) {
            std::string output_id = id();
            if( auto r = this->template res<reflect::DynamicObject>(i); r) {
                if (function.num_outputs() > 1) {
                    output_id += "[" + std::to_string(i) + "]";
                }
                r->set_id(output_id);
            } 
        }
    }

    /**
     * @brief This function evaluates the wrapped function and caches the
     * output.
     * 
     */
    void eval() const override
    {
        // tranform input nodes to vector of DynamicObjects
        std::vector<reflect::DynamicObject> inputs_vec;
        inputs_vec.reserve(this->num_parents());
        for (int i = 0; i < this->num_parents(); ++i) {
            inputs_vec.push_back(this->arg<reflect::DynamicObject>(i).value().as_const());
        }

        // call the wrapped function
        auto ret = function.invoke(inputs_vec);

        // transform to outputs
        for (int i=0; i < this->num_children(); ++i) {
            if (auto output = this->template res<reflect::DynamicObject>(i); output) {
                output->set_value(ret[i]);
            }
        }
    }

    /**
     * @brief serialize a DynamicAction to yaml, for use in a grunk recipe. It will be
     * serialized using the function name as a tag. The yaml node consists of a list
     * with two elements. The first element is a list containing the ids of the output
     * ::grunk::DynamicFeature instances. The second element is a list containing the
     * ids of the input ::grunk::DynamicFeature instances.
     * @return A yaml-string representing the DynamicAction
     */
    std::string serialize() const override final
    {

        YAML::Node y;

        // outputs
        auto const& outputs = this->template res<0>();
        y.push_back(YAML::Node());
        for (int i = 0; i < this->num_children(); ++i){
            if(auto const& output = this->res<reflect::DynamicObject>(i); output)
                y[0].push_back(output->id());
        }

        // inputs
        y.push_back(YAML::Node());
        for (int i = 0; i < this->num_parents(); ++i){
            auto const& input = this->arg<reflect::DynamicObject>(i);
            
            if ( input.num_parents() == 0 && input.id() == "") {
                // handle constants, i.e. parent-less nodes with id "":
                YAML::Node n = YAML::Load(input.serialize());
                y[1].push_back(n);
            } else {
                // push the name of the input
                y[1].push_back(input.id());
            }
        }
        y.SetStyle(YAML::EmitterStyle::Flow);
        YAML::Emitter out;

        auto tag = YAML::VerbatimTag(function.get_full_name());
        out << tag << y;
        return out.c_str();
    }

    /**
     * @brief deserializes a yaml-representation of a DynamicAction , e.g. from a
     * grunk recipe, to an instance of ::grunk::DynamicAction.
     *
     * @return The list of return values wrapped in DynamicFeature instances
     */
    static ResultHolder<Action<reflect::DynamicFunction>> deserialize(
        YAML::Node const&,
        Recipe const&
    );

private:
    reflect::DynamicFunction const& function;
};

/**
 * @brief typedef for an Action wrapping a DynamicFunction
 * @ingroup dynamic_advanced
 */
using DynamicAction = Action<reflect::DynamicFunction>;

/**
 * @ingroup dynamic_advanced
 * @brief Specialization of the ResultHolder class template for DynamicActions. It is a proxy for holding the 
 * result of a ::grunk::DynamicAction instance. The results is an std::vector of DynamicFeatures. 
 * In addition to storing the result, it stores a const reference to the compute_node so that void functions
 * can be evaluated as well.
 * 
 */
template <>
class ResultHolder<DynamicAction> {
    using result_type = std::vector<DynamicFeature>;

public:

    ResultHolder(result_type const& res, std::shared_ptr<parametric::DAGNode> const& c) : result(res), m_compute_node(c) {}

    /**
     * @brief returns the i-th output 
     * 
     * @param i index of the queried output
     * @return decltype(auto) the -ith output DynamicFeature
     */
    decltype(auto) output(int i=0) const {
        return result[i];
    }

    /**
     * @brief returns the number of outputs
     * 
     * @return constexpr size_t the number of outputs
     */
    size_t size() const {
        return result.size();
    }

    /**
     * @brief retunrs a const reference to the DynamicAction
     * 
     * @return DynamicAction const& the DynamicAction instance
     */
    std::shared_ptr<parametric::DAGNode> const& compute_node() const {
        return m_compute_node;
    }

    /**
     * @brief evaluates the DynamicAction
     * 
     */
    void eval() const {
        compute_node()->eval();
    }


private:
    std::shared_ptr<parametric::DAGNode> m_compute_node;
    result_type result;
};

namespace details {

/**
 * @brief The ActionFactory struct is an internal factory for creating Action
 * instances.
 *
 * It is a proxy class used in the free factory functions action. Factory functions are
 * needed, because Actions should always be wrapped in a parametric::compute_node_ptr
 * and the private constructor of Action makes sure that there is no misuse. The
 * factory function parametric::new_node does not work with the templated constructors of the
 * Action class, so we need new factory functions.
 *
 * The proxy factory is needed, because the factory functions action must be templated, and
 * templated friend functions are a pain in the ass. This way we have a non-templated friend
 * struct with templated member functions.
 */
struct DynamicActionFactory
{

    /**
     * @brief Returns a new DynamicActionPtr given a DynamicFunction and an
     * vector of DynamicFeatures
     * 
     * @param id The id of the output ::grunk::DynamicFeature
     * @param fun The reflect::DynamicFunction to be wrapped
     * @param args The input features
     * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
     */
    static ResultHolder<DynamicAction> new_action(
        std::string const& id, 
        reflect::DynamicFunction const& fun, 
        std::vector<DynamicFeature> const& args
    )
    {
        auto ptr = std::shared_ptr<DynamicAction>(new DynamicAction(id, fun));

        return ResultHolder<DynamicAction>(
            parametric::compute(ptr, args), 
            ptr
        );

    }

};

} //namespace details

/**
 * @brief Given a function and a vector of features in the feature 
 * tree, this function creates a DynamicAlgoritm instance representing the 
 * evaluation of the input function for the input features.
 * 
 * @tparam F The type of the function to be wrapped. This can be any referentially transparent function, 
             In particular, the function must be invokable on const 
             references.
 * @param id The id of the output ::grunk::DynamicFeature
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder A special wrapper class around the output features.
 * @ingroup dynamic_advanced
 */
ResultHolder<DynamicAction> action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args);

namespace details {

template<typename T>
struct is_string
        : public std::disjunction<
                    std::is_same<char *, std::decay_t<T>>,
                    std::is_same<const char *, std::decay_t<T>>,
                    std::is_same<std::string, std::decay_t<T>>,
                    std::is_same<std::string_view, std::decay_t<T>>
        > 
{};

template <typename T>
constexpr bool is_string_v = is_string<T>::value;

template <typename Arg>
DynamicFeature to_dynamic_feature(Arg&& arg)
{
    using T = std::decay_t<Arg>;
    if constexpr (details::is_feature_v<T>){
        return std::forward<Arg>(arg);
    } else {
        // special handling of string-like types: We want to always conert them to String first
        if constexpr (details::is_string_v<Arg>) {
            return Feature("", reflect::DynamicObject(helper::String(std::forward<Arg>(arg))));
        } else {
            return Feature("", reflect::DynamicObject(std::forward<Arg>(arg)));
        }
    }
}

} // namespace details

/**
 * @brief Given a function and some features in the feature tree, this 
 * function creates a DynamicAlgoritm instance representing the evaluation
 * of the input function for the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 * 
 * @tparam Args The types of the arguments expected by the input function
 * @param id The id of the output ::grunk::DynamicFeature
 * @param fun The input function
 * @param args The input features of the feature tree
 * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
 * @ingroup dynamic_advanced
 */
template <
    typename... Args,
    typename = std::enable_if_t<!(sizeof...(Args) == 1 && (std::is_same_v<std::vector<DynamicFeature>, std::decay_t<Args>> && ...))>
>
ResultHolder<DynamicAction> action(std::string const& id, reflect::DynamicFunction const& fun, Args&&... args)
{
    return action(
        id, 
        fun, 
        std::vector<DynamicFeature>{details::to_dynamic_feature(std::forward<Args>(args))...}
    );
}

namespace details {

template <typename T>
reflect::DynamicFunction::SpecifiedArgument to_specified_argument(T&& f)
{
    using F = std::decay_t<T>;
    reflect::DynamicFunction::ArgumentSpecifier spec 
        = reflect::DynamicFunction::ArgumentSpecifier::PtrOrRefToConst;
    if constexpr (std::is_rvalue_reference_v<T>) {
        spec = reflect::DynamicFunction::ArgumentSpecifier::Value;
    }

    if constexpr ( std::is_same_v<F, DynamicFeature>) {
        if (f.get_type_descriptor() == nullptr) {
            throw std::logic_error("Unexpected error: Unknown type of dynamic feature.");
        }
        return reflect::DynamicFunction::SpecifiedArgument{
            f.get_type_descriptor(),
            spec
        };
    } else if constexpr (details::is_feature_v<F>) {
        return reflect::DynamicFunction::SpecifiedArgument{
            reflect::resolve<typename F::value_type>(),
            spec
        };
    } else {
        return reflect::DynamicFunction::SpecifiedArgument{
            reflect::resolve<F>(),
            spec
        };
    }
}

} // namespace details

/**
 * @brief Given a string identifier of a function, that has previously been registered
 * in the static function registry as well as input features of the feature tree, 
 * this function represents the evaluation of the registered function when it gets 
 * passed the input features.
 * 
 * @tparam Args The types of the arguments expected by the registered function
 * @param id The id of the output ::grunk::DynamicFeature
 * @param name The string identifier of the registered function
 * @param args The input Features
 * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
 *
 * @ingroup dynamic
 */
template <typename... Args>
ResultHolder<DynamicAction> action(std::string const& id, std::string const& name, Args&&... args)
{
    std::vector<reflect::DynamicFunction::SpecifiedArgument> specified_args{
        details::to_specified_argument(std::forward<Args>(args))...
    };

    auto const& overload = reflect::resolve_function(name);
    auto const& function = overload.resolve(specified_args);
    return action(id, function, std::forward<Args>(args)...);
}

ResultHolder<DynamicAction> action(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args);

} // namespace grunk
