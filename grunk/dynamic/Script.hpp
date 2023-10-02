#pragma once 

#include <grunk/dynamic/DynamicFeature.hpp>

#include <grunk/parametric_core.hpp>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include <initializer_list>
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

namespace details {
    struct ignore {};
}

class Script : public parametric::ComputeNode<Script, details::ignore, details::ignore>
{
public:

    using VariableMap = std::unordered_map<std::string, reflect::DynamicObject>;
    using ResultType = std::vector<DynamicFeature>;

    struct Step 
    {
        using Argument = std::variant<
            std::string, 
            DynamicFeature,
            int
        >;

        Step(
            std::string fun,
            std::initializer_list<std::string> const& outputs_,
            std::initializer_list<Argument> const& inputs
        );

        std::string function_name;
        std::vector<Argument> arguments;
        std::vector<std::string> outputs;
    };

    friend ResultHolder<DynamicAction> script(
        std::initializer_list<Step> const&,
        std::initializer_list<std::string> const&
    );

    void connect_inputs(std::initializer_list<Step> const&);
    ResultType initialize_results() const;
    void connect_results(Script::ResultType const&);
    void post_connect() const;
    void eval() const override final;

private:

    Script(
        std::initializer_list<Step> const& steps,
        std::initializer_list<std::string> const& returns
    );

    void eval(Step const& s, VariableMap& vars) const;

    std::vector<Step> mutable steps;
    std::vector<std::string> const returns;
};

ResultHolder<DynamicAction> script(
    std::initializer_list<Script::Step> const& steps,
    std::initializer_list<std::string> const& returns
);

}