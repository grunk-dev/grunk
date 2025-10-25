#pragma once 

#include "grunk/dynamic/feature.hpp"
#include "grunk/dynamic/function_meta.hpp"
#include "ResultHolder.hpp"
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
 * @ingroup dynamic_advanced
 */
class ActionDynamic : public parametric::ComputeNode<
                                 ActionDynamic,
                                 parametric::Results<object>, /* Results are ignored*/
                                 parametric::Arguments<std::vector<object>> /* Arguments are ignored by derived class */
                             >
{

    friend struct details::DynamicActionFactory;

private:

    /**
     * @brief Construct a new DynamicAction given a DynamicFunction<F> and 
     * an std::vector of DynamicFeatures.
     * 
     * @param fun A const pointer to a reflect::Function
     * @param in The input DynamicFeatures
     */
    ActionDynamic(function_meta const& fun)
     : function(fun)
    {}

    inline decltype(auto) result() const {
        return this->template res<object>(0);
    }

    inline decltype(auto) argument(int i) const {
        return this->template arg<object>(i);
    }

public:


    void connect_inputs(std::vector<DynamicFeature> const& args)
    {
        for (auto const& arg : args) {
            depends_on(arg);
            evaluators.push_back(make_evaluator(arg));
        }
    }

    template <typename... Args>
    void connect_inputs(Feature<Args> const&... args){
        (depends_on(args), ...);
        (evaluators.push_back(make_evaluator(args)), ...);
    }

    DynamicFeature initialize_results() const
    {
        return feature<object>({});
    }

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
 * @ingroup dynamic_advanced
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

    ResultHolder(result_type const& res, std::shared_ptr<parametric::DAGNode> const& c) : result(res), m_compute_node(c) {}

    /**
     * @brief returns the i-th output 
     * 
     * @param i index of the queried output
     * @return decltype(auto) the -ith output DynamicFeature
     */
    decltype(auto) output() const {
        return result;
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

} //namespace grunk
