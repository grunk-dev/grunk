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
