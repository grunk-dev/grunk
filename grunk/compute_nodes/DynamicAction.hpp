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

#include <grunk/compute_nodes/Action.hpp>
#include <grunk/DynamicFeature.hpp>
#include <grunk/common/String.hpp>

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

} // namespace grunk
