#include "io.h"

#include <grunk/dynamic/RuntimeAlgorithm.h>

namespace grunk {

io_error::io_error(std::string const& msg)
 : mMessage("grunk IO error: "s + msg)
{}

const char* io_error::what() const noexcept
{
    return mMessage.c_str();
}

std::string io_error::get_message() const
{
    return mMessage;
}

namespace details {

Reflect::DynamicObject deserialize(
    std::string const& type_name,
    YAML::Node const & yaml_node
)
{
    Reflect::TypeDescriptor const* descr = Reflect::Resolve(type_name);
    if (!descr){
        // throw an error
        throw io_error("Unknown type "s + type_name);
    }
    auto const* deserialize = descr->GetMemberFunction("deserialize");
    if (!deserialize) {
        throw io_error("type "s + type_name + " does not have a (static) \"deserialize\" method. Please refer to the grunk documentation");
    }
    try {
        return (*deserialize)(yaml_node)[0];
    }
    catch (std::exception& e) {
        throw io_error(
            "Could not deserialize field \"value\"\n\n" + Dump(yaml_node)
            + "\n\nto an instance of type \"" + type_name
            + "\". Caught an exception with description: \""
            + e.what() + "\" while trying."
        );
    }
}

FeatureContainer yaml_to_feature_tree(YAML::Node const& root)
{
    if (!root["uses"]) {
        throw io_error("Missing \"uses\" block.");
    }

    auto const uses = root["uses"];
    if (!uses["grunk"])
    {
        throw io_error("Missing \"grunk\" in \"uses\" block.");
    }

    try {
        auto ver = uses["grunk"].as<std::string>();
        if (ver != grunk_VERSION) {
            //TODO: Generate a meaningful warning. Throwing an exception is not a 
            // viable solution. This will be done here anyway as long as grunk is in experimental state.
            throw io_error("Parsed version "s + ver + " does not match grunk version " + grunk_VERSION);
        }
    }
    catch (...) 
    {
        throw io_error("could not parse grunk version.");
    }

    //TODO: Parse plugins from input file and compare with loaded plugins. Handle appropriately

    FeatureContainer features;

    if (auto const parameters = root["parameters"]; parameters) {
        for (YAML::const_iterator it=parameters.begin();it!=parameters.end();++it) {
            auto name = it->first.as<std::string>();

            auto type = it->second["type"];
            if (!type) {
                throw io_error("Missing field \"type\" for parameter "s + name + ".");
            }

            auto value = it->second["value"];
            if (!value) {
                throw io_error("Missing field \"value\" for parameter "s + name + ".");
            }

            auto object = deserialize(type.as<std::string>(), value);
            features.emplace(name, RuntimeFeature(name, std::move(object)));
        }
    }

    if (auto const steps = root["steps"]; steps)
    {
        for (YAML::const_iterator it=steps.begin(); it!=steps.end(); ++it) {
            
            auto const function = (*it)["function"];
            if (!function) {
                throw io_error("Currently, only \"function\" steps are supported");
            }
            auto const function_name = function.as<std::string>();

            std::vector<RuntimeFeature> input_vec;
            auto const inputs = (*it)["inputs"];
            if (inputs) {
                for (YAML::const_iterator inputs_it=inputs.begin(); inputs_it!=inputs.end(); ++inputs_it) {
                    auto input_name = (*inputs_it).as<std::string>();
                    auto feature_it = features.find(input_name);
                    if (feature_it == std::end(features)) {
                        throw io_error(
                            "Could not find input "s
                             + input_name + " for function call to " + function_name
                             + ". Are the steps in the correct topological order?"
                        );
                    }
                    input_vec.push_back(feature_it->second);
                }
            }

            auto comp_node = eval("", function_name, input_vec);

            auto const outputs = (*it)["outputs"];
            if (!outputs)
            {
                throw io_error("No outputs specified for function call of "s + function_name);
            }
            if (!outputs.size() == comp_node->number_of_outputs()) {
                throw io_error("Number of given outputs doesn't match number of outputs of function "s + function_name);
            }

            size_t idx = 0;
            for (YAML::const_iterator outputs_it=outputs.begin(); outputs_it!=outputs.end(); ++outputs_it) {
                auto output_name = (*outputs_it).as<std::string>();
                auto output = comp_node->get(idx++);
                output.set_id(output_name);
                features.emplace(output_name, output);
            }


        }
    }

    return features;
}

} // namespace details 

FeatureContainer read(std::string filename)
{
    auto const root = YAML::LoadFile(filename);
    try {
        return details::yaml_to_feature_tree(root);
    } catch(const io_error& e)
    {
        throw io_error(std::string(e.get_message()) + " filename = " + filename);
    }
}

} // namespace grunk