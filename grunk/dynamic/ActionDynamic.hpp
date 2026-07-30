// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include "grunk/dynamic/DynamicFeature.hpp"
#include "grunk/dynamic/function_meta.hpp"
#include "grunk/core/ResultHolder.hpp"
#include "sol/sol.hpp"

namespace grunk {

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
 * @ingroup advanced_dynamic
 */
class ActionDynamic : public parametric::ComputeNode<ActionDynamic>
{

    friend struct details::DynamicActionFactory;

private:

    /**
     * @brief Construct a new DynamicAction given a DynamicFunction<F> and 
     * an std::vector of DynamicFeatures.
     * 
     * @param fun a function with meta data to be wrapped as an action
     */
    ActionDynamic(function_meta const& fun)
     : function(fun)
    {}

    /**
     * @brief returns the result of this action
     */
    inline decltype(auto) result() const {
        return this->template res<object>(0);
    }

    /**
     * @brief returns the ith argument of this action
     * 
     * @param i the index of the argument
     */
    inline decltype(auto) argument(int i) const {
        return this->template arg<object>(i);
    }

public:

    /**
     * @brief connect the inputs to the compute node represented by this action
     *
     * @param args the input features
     */
    void connect_inputs(std::vector<DynamicFeature> const& args)
    {
        for (auto const& arg : args) {
            depends_on(arg);
            evaluators.push_back(make_evaluator(arg));
        }
    }

    /**
     * @brief connect the inputs to the compute node represented by this action
     *
     * @tparam Args the types of the inputs features
     * @param args the input features
     */
    template <typename... Args>
    void connect_inputs(Feature<Args> const&... args){
        (depends_on(args), ...);
        (evaluators.push_back(make_evaluator(args)), ...);
    }

    /**
     * @brief initializes an empty feature for the output of this action. If the wrapped
     * function's C++ return type was known statically at registration time (see
     * function_meta::return_type_hint), the output feature is tagged with that type hint
     * right away - before the function has ever run - so that later method lookups on it
     * (see DynamicFeature's native colon-call dispatch) never need to force evaluation.
     *
     * Built via function.lua_state() rather than feature<object>({}) (which would
     * construct from a default sol::object - lua_state() nullptr until evaluated) -
     * the function's own Lua state is always valid and known without evaluating
     * anything, and connect_results()/eval() below overwrite this placeholder's value
     * unconditionally once the action actually runs, so its initial value never matters,
     * only that its lua pointer is usable right away (e.g. by DynamicFeature::call()).
     */
    DynamicFeature initialize_results() const
    {
        DynamicFeature out(function.lua_state());
        if (auto hint = function.return_type_hint()) {
            out.set_type_hint(*hint);
        }
        return out;
    }

    /**
     * @brief connects this compute node with the inputs and outputs
     */
    void connect_results(DynamicFeature const& res)
    {
        computes(res);
    }

    /**
     * @brief This function evaluates the wrapped function and caches the
     * output.
     * 
     */
    void eval() const override
    {
        // tranform input nodes to vector of DynamicObjects
        std::vector<object> inputs_vec;
        inputs_vec.reserve(this->num_parents());
        for (size_t i = 0; i < this->num_parents(); ++i) {
            auto const& parent = this->get_parents()[i];
            inputs_vec.push_back(evaluators[i](*parent));
        }

        // call the wrapped function
        sol::protected_function_result res = function.call(sol::as_args(inputs_vec));
        if (!res.valid()) {
            sol::error err = res;
            throw std::runtime_error(std::string("Error evaluting dynamic action: ") + err.what());
        }

        // transform to output
        if (auto output = result(); output) {
            output->set_value(res[0]);
        }
    }

    /**
     * @brief serialize a DynamicAction to string
     *
     * @return std::string the serialized DynamicAction
     */
    std::string serialize() const override final
    {

        std::string ret;
        bool is_anonymous = false;

        // output
        if(auto const& output = result(); output) {
            if (!output->id().empty()) {
            ret += output->id() + " = ";
            } else {
                is_anonymous = true;
            }
        } else {
            return "";
        }

        //function name
        std::string func_name = function.get_name();

        bool is_operator = (func_name.rfind("grunk._dynamic_", 0) == 0);
        std::string argument_seperator = "";
        if (!is_operator) {
            // regular function call syntax
            ret += func_name + "(";
            argument_seperator = ", ";
        } else {

            if (is_anonymous) {
                ret += "(";
            }

            if (func_name == "grunk._dynamic_add") {
                argument_seperator = " + ";
            } else if (func_name == "grunk._dynamic_sub") {
                argument_seperator = " - ";
            } else if (func_name == "grunk._dynamic_mul") {
                argument_seperator = " * ";
            } else if (func_name == "grunk._dynamic_div") {
                argument_seperator = " / ";
            } else if (func_name == "grunk._dynamic_pow") {
                argument_seperator = " ^ ";
            } else if (func_name == "grunk._dynamic_mod") {
                argument_seperator = " % ";
            } else if (func_name == "grunk._dynamic_unm") {
                ret += "-";
            } else {
                throw std::logic_error("Unknown operator function in DynamicAction serialization.");
            }
        }

        // inputs
        auto serialize_arg = [](parametric::DAGNode const& node) -> std::string
        {
            bool is_anonymous = (node.id() == "");
            bool is_constant = (node.num_parents() == 0 && is_anonymous);
            if (is_constant) {
                return node.serialize();
            } else if (is_anonymous) {
                // nested function call
                return node.get_parents()[0]->serialize();
            } else {
                // a named feature
                return node.id();
            }
        };

        bool first_arg = true;
        for (auto const& input : this->get_parents()){
            if (first_arg) {
                first_arg = false;
            } else {
                ret += argument_seperator;
            }
            ret += serialize_arg(*input);
        }
        if (!is_operator || is_anonymous) {
            ret += ")";
        }

        return ret;
    }


private:

    // using DAGNodeToObj = object(*)(parametric::DAGNode const&);
    using DAGNodeToObj = std::function<object(parametric::DAGNode const&)>;

    template <typename T>
    DAGNodeToObj make_evaluator(Feature<T> const& f) {
        if constexpr (std::is_same_v<T, object>) {
            return [](parametric::DAGNode const& node){
                return dynamic_cast<parametric::impl::param_holder<object> const&>(node).value();
            };
        } else {
            //TODO: Can this code be reached? Is this an artifact from the pre-LUA era of grunk?
            auto* lua_state = function.lua_state();
            return [lua_state](parametric::DAGNode const& node) -> sol::object {
                auto value = dynamic_cast<parametric::impl::param_holder<T> const&>(node).value();
                return sol::make_object(lua_state, value);
            };
        }
    }

    // store a vector of functions, that evaluate a (parent) DAGNode to a DynamicObject
    std::vector<DAGNodeToObj> evaluators;

    function_meta const function;
};

/**
 * @ingroup advanced_dynamic
 * @brief Specialization of the ResultHolder class template for DynamicActions. It is a proxy for holding the 
 * result of a ::grunk::DynamicAction instance. The results is an std::vector of DynamicFeatures. 
 * In addition to storing the result, it stores a const reference to the compute_node so that void functions
 * can be evaluated as well.
 * 
 */
template <>
class ResultHolder<ActionDynamic> {
    using result_type = DynamicFeature;

public:

    /**
     * @brief construct a new ResultHolder instance connecting an output dynamic feature to a compute node
     *
     * @param res the output dynamic feature
     * @param c the compute node
     */
    ResultHolder(result_type const& res, std::shared_ptr<parametric::DAGNode> const& c) : result(res), m_compute_node(c) {}

    /**
     * @brief returns the i-th output 
     * 
     * @return decltype(auto) the -ith output DynamicFeature
     */
    decltype(auto) output() const {
        return result;
    }

    /**
     * @brief returns a const reference to the DynamicAction
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
 *
 * @ingroup advanced_dynamic
 */
struct DynamicActionFactory
{

    /**
     * @brief Returns a new DynamicActionPtr given a DynamicFunction and an
     * vector of DynamicFeatures
     * 
     * @param fun The reflect::DynamicFunction to be wrapped
     * @param args The input features
     * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
     */
    static ResultHolder<ActionDynamic> new_action(
        function_meta const& fun, 
        std::vector<DynamicFeature> const& args
    )
    {
        auto ptr = std::shared_ptr<ActionDynamic>(new ActionDynamic(fun));

        return ResultHolder<ActionDynamic>(
            parametric::compute(ptr, args), 
            ptr
        );

    }

    /**
     * @brief Returns a new DynamicActionPtr given a DynamicFunction and an
     * vector of DynamicFeatures
     * 
     * @param fun The reflect::DynamicFunction to be wrapped
     * @param args The input features
     * @return ResultHolder<DynamicAction> The returned ResultHolder wrapping the outputs
     */
    template <typename... Args>
    static ResultHolder<ActionDynamic> new_action(
        function_meta const& fun, 
        Feature<Args> const&... args
    )
    {
        auto ptr = std::shared_ptr<ActionDynamic>(new ActionDynamic(fun));

        return ResultHolder<ActionDynamic>(
            parametric::compute(ptr, args...), 
            ptr
        );
    }

};

} //namespace details

/**
 * @brief Given a string identifier of a function, that has previously been registered
 * in the static function registry as well as input features of the feature tree,
 * this function represents the evaluation of the registered function when it gets
 * passed the input features.
 *
 * @tparam Args The types of the arguments expected by the registered function
 * @param function The function with metadata
 * @param args The input Features
 * @return DynamicFeature the result
 *
 * @ingroup dynamic
 */
template <typename... Args>
inline DynamicFeature action(function_meta const& function, Args&&... args)
{
    using FirstArg = std::tuple_element<0, std::tuple<Args...>>;
    if constexpr (sizeof...(Args) == 1 && (std::is_same_v<std::decay_t<Args>, std::vector<DynamicFeature>> || ...)) {
        return details::DynamicActionFactory::new_action(function, args...).output();
    } else {
        return details::DynamicActionFactory::new_action(function, details::to_feature(std::forward<Args>(args))...).output();
    }
}

namespace details {

/**
 * @brief make_dynamic_action is a helper function to create a lambda function that can be registered in the grunk state as a DynamicFunction. The returned lambda takes variadic arguments, converts them to DynamicFeatures and calls the action factory to create a DynamicAction.
 * 
 * @param func the function_meta of the function to be wrapped in the lambda
 * @return auto a lambda function that can be registered as a DynamicFunction in the grunk state
 *
 * @ingroup advanced_dynamic
 */
inline auto make_dynamic_action(function_meta const& func)
{
    auto decorated_function = [func](sol::variadic_args va) -> grunk::DynamicFeature
    {
        
        auto raw_args = std::vector<sol::object>(va.begin(), va.end());

        // HACK: for unary operators, sol::variadic_args includes the table as the first argument
        // For serialization/deserialization consistency, we remove it here
        // Otherwise -x gets serializes as -xx
        if (func.get_name() == "grunk._dynamic_unm") {
            raw_args.erase(raw_args.begin());
        }

        std::vector<grunk::DynamicFeature> args;
        args.reserve(raw_args.size());

        // Use std::transform to convert variadic_args to std::vector<DynamicFeature>
        std::transform(
            raw_args.begin(), raw_args.end(),
            std::back_inserter(args),
            [](grunk::object const& obj) {
                if (obj.is<grunk::DynamicFeature>()) {
                    return obj.as<grunk::DynamicFeature>();
                } else {
                    return grunk::feature(obj);
                }
            }
            );

        return grunk::action(func, args);
    };
    return decorated_function;
}

} // namespace details

} //namespace grunk