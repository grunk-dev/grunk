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
            if (s[0].Type() == YAML::NodeType::Sequence) {
                for (auto const& output : s[0]) {
                    // handle sequence of ids
                    std::string id = output.as<std::string>();
                    if (ids.find(id) != ids.end()) {
                        return false;
                    }
                    ids[id] = true;
                }
            } else if (s[0].Type() == YAML::NodeType::Map) {
                // handle map for output ids. output ids are the keys (e.g. in Recipe::Action)
                for(YAML::const_iterator it=s[0].begin();it!=s[0].end();++it) {
                    std::string id = it->first.as<std::string>();
                    if (ids.find(id) != ids.end()) {
                        return false;
                    }
                    ids[id] = true;
                }
            } else {
                std::string id = s[0].as<std::string>();
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
