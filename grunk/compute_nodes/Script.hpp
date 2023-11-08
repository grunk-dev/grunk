#pragma once 

#include <grunk/DynamicFeature.hpp>
#include <grunk/Recipe.hpp>
#include <grunk/parametric_core.hpp>

#include <yaml-cpp/yaml.h>

#include <unordered_map>
#include <vector>
#include <variant>

namespace grunk {

/**
 * @brief An exception representing errors with grunk's file I/O system
 */
class script_error : public std::exception
{
public: 
    /**
     * @brief Construct a new io error object from an error message
     * 
     * @param msg 
     */
    script_error(std::string const& msg);

    /**
     * @brief print the error message with the prefix "grunk IO error"
     * 
     * @return const char* the error message
     */
    const char *what() const noexcept override;

    /**
     * @brief Get the error message without the prefix "grunk IO error"
     * 
     * @return std::string the error message
     */
    std::string get_message() const;
private:
    std::string mMessage;
};

/**
 * @brief The Script class represents a sequence of function calls that are all evaluated as 
 * part of a single compute node. 
 *
 * The class has a private constructor, as it should always be created with the factory function
 * ::grunk::script
 *
 * @ingroup dynamic_advanced
 * 
 */
class Script : public parametric::ComputeNode<Script>
{
public:

    using VariableMap = std::unordered_map<std::string, reflect::DynamicObject>;
    using ResultType = std::vector<DynamicFeature>;

    /**
     * @brief This class represents a single step of a Script compute node. 
     */
    struct Step 
    {
        using Argument = std::variant<
            std::string,        // reference a variable created previously within the script
            DynamicFeature,     // reference a feature created previously outside of script (input to compute node)
            int                 // DynamicFeature Arguments will internally be replaced by the index of the parent
        >;

        /**
         * @brief Construct a new Step object
         * 
         * @param fun The name of a function loaded from a plugin
         * @param outputs_ names to be assigned to the outputs of the functions, if any
         * @param inputs inputs of the function. Can be either a DynamicFeature defined previously outside of the script or 
         *               the name of a variable craeted as part of the script.
         */
        Step(
            std::string fun,
            std::vector<std::string> const& outputs_,
            std::vector<Argument> const& inputs
        );

        /**
         * @brief deserializes a Step from yaml.
         * 
         * @param node The yaml node
         * @param features A map of features in the recipe
         * @return Step A new Step instance
         */
        static Step deserialize(YAML::Node const& node, Recipe::FeatureContainer& features);

        std::string function_name;
        std::vector<Argument> arguments;
        std::vector<std::string> outputs;
    };

    friend ResultHolder<DynamicAction> script(
        std::vector<Step> const&,
        std::vector<std::string> const&
    );

    /**
     * @brief Parses the arguments of a list of steps and connects all referenced 
     * DynamicFeatures as parents of this compute node.
     * 
     */
    void connect_inputs(std::vector<Step> const&);

    /**
     * @brief Initializes a map of output features that stores the outputs as 
     * defined in the outputs vector passed to the constructor.
     * 
     * @return ResultType a map of DynamicFeatures
     */
    ResultType initialize_results() const;

    /**
     * @brief connects the DynamicFeatures of the output map to this compute node as children
     * 
     */
    void connect_results(Script::ResultType const&);

    /**
     * @brief assigns the ids to the output nodes after connection
     * 
     */
    void post_connect() const;

    /**
     * @brief evaluates the script and stores the result in the output map
     * 
     */
    void eval() const override final;

    /**
     * @brief serializes a Script instance to yaml
     * 
     * @return std::string 
     */
    std::string serialize() const override final;

    /**
     * @brief deserializes a yaml node representing a script to the evaluation of
     * the script.
     * 
     * @param node yaml node 
     * @return ResultHolder<DynamicAction> The returned DynamicFeatures
     */
    static ResultHolder<DynamicAction> deserialize(YAML::Node const& node, Recipe::FeatureContainer&);

private:

    /**
     * @brief Construct a new Script object given a vector of steps as well as a vector of 
     * ids for the return features. These ids must correspond with variables created as part of the script.
     * 
     * @param steps 
     * @param returns 
     */
    Script(
        std::vector<Step> const& steps,
        std::vector<std::string> const& returns
    );

    /**
     * @brief evaluates a single step and stores the result in a map of DynamicObjects
     * 
     * @param s a Script::Step instance
     * @param vars A map of DynamicObjects, storing all intermediate variables of the script
     */
    void eval(Step const& s, VariableMap& vars) const;

    /**
     * @brief serializes a single step to yaml
     * 
     * @param s
     * @return YAML::Node 
     */
    YAML::Node serialize(Step const& s) const;

    std::vector<Step> mutable steps;
    std::vector<std::string> const returns;
    std::vector<reflect::TypeDescriptor const*> return_types;
};

/**
 * @brief Given a sequence of steps, each representing a function call of a dynamic function, 
 * as well as a list of output ids of intermediate variables, this function creates a compute node
 * that represents the evaluation of these steps within a compute node of a parametric tree. 
 *
 * This is useful, if
 *  * some operations should be performed without intermediate lazy evaluation and caching
 *  * we want to create a class and modify it using non-const setter methods.
 *
 *
 * @param steps 
 * @param returns 
 * @return ResultHolder<DynamicAction> 
 */
ResultHolder<DynamicAction> script(
    std::vector<Script::Step> const& steps,
    std::vector<std::string> const& returns
);

}
