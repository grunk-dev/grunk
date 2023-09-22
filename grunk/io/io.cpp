#include "io.hpp"

#include <grunk/dynamic/DynamicAction.hpp>
#include <grunk/dynamic/Expression.hpp>
#include <grunk/dynamic/Recipe.hpp>

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

bool has_unique_feature_names(YAML::Node const& root){

    std::unordered_map<std::string, bool> ids;
    
    if (root["parameters"]) {
        for (auto const& p : root["parameters"]) {
            std::string id = p.first.as<std::string>();
            ids[id] = true;
        }
    }

    if (root["steps"]) {
        for (auto const& s : root["steps"]) {
            for (auto const& output : s[0]) {
                std::string id = output.as<std::string>();
                if (ids.find(id) != ids.end()) {
                    return false;
                }
                ids[id] = true;
            }
        }
    }

    return true;
}

reflect::DynamicObject deserialize(
    std::string const& type_name,
    YAML::Node const & yaml_node
)
{
    reflect::TypeDescriptor const* descr = nullptr;
    try {
        descr = reflect::resolve(type_name);
    } catch(std::out_of_range const& e)
    {
        // convert to io error
        throw io_error(e.what());
    }

    if (!descr) {
        throw io_error("Failed to resolve type with name\""s + type_name + "\".");
    }

    auto deserializer = descr->get_member_function("deserialize", reflect::to_optional_tag);
    if (!deserializer) {
        throw io_error("type "s + type_name + " does not have a (static) \"deserialize\" method. Please refer to the grunk documentation");
    }
    try {
        return (**deserializer)(yaml_node)[0];
    }
    catch (std::exception& e) {
        throw io_error(
            "Could not deserialize yaml node\n\n" + Dump(yaml_node)
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
    catch (std::exception const& e) 
    {
        throw io_error(e.what());
    }

    //TODO: Parse plugins from input file and compare with loaded plugins. Handle appropriately

    FeatureContainer features;

    if (auto const parameters = root["parameters"]; parameters) {
        for (YAML::const_iterator it=parameters.begin();it!=parameters.end();++it ) {
            
            auto name = it->first.as<std::string>();
            auto type = it->second.Tag();
            auto value = it->second;

            if (features.find(name) != features.end()) {
                throw io_error("Error parsing parameters. A parameter with name \"" + name + "\" already exists.");
            }

            auto object = deserialize(type, value);
            features.emplace(name, DynamicFeature(name, std::move(object)));
        }
    }

    if (auto const steps = root["steps"]; steps)
    {
        for (size_t i = 0; i < steps.size(); i++) {
            
            auto const function_name = steps[i].Tag();

            if (function_name == "expr") {

                auto output = Expression::deserialize(steps[i], features);
                auto output_name = steps[i][0].as<std::string>();
                if (features.find(output_name) != features.end()) {
                    throw io_error("Error parsing step " + std::to_string(i) + ": A parameter with name \"" + output_name + "\" already exists.");
                }
                output.set_id(output_name);
                features.emplace(output_name, output);

            } else {


                auto output_nodes = DynamicAction::deserialize(steps[i], features);

                auto const outputs = steps[i][0];
                if (outputs.size() != output_nodes.size()) {
                    throw io_error("Number of given outputs doesn't match number of outputs of function "s + function_name);
                }

                size_t idx = 0;
                for (auto const& node : outputs) {
                    auto output_name = node.as<std::string>();

                    if (features.find(output_name) != features.end()) {
                        throw io_error("Error parsing step " + std::to_string(i) + ": A parameter with name \"" + output_name + "\" already exists.");
                    }

                    auto output = output_nodes.output(idx++);
                    output.set_id(output_name);
                    features.emplace(output_name, output);
                }

            }


        }
    }

    return features;
}

} // namespace details 

std::string to_string(Recipe const& r)
{
    YAML::Emitter out;
    out << r.serialize();
    return out.c_str();
}

void write(std::string const& filename, Recipe const& recipe)
{
    std::ofstream fout(filename);
    fout << to_string(recipe) << "\n";
}

Recipe read(std::string filename)
{
    auto const root = YAML::LoadFile(filename);
    try {
        return Recipe::deserialize(root);
    } catch(const io_error& e)
    {
        throw io_error(std::string(e.get_message()) + " filename = " + filename);
    }
}

} // namespace grunk
