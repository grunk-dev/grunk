#include <grunk/common/io.hpp>

#include <grunk/compute_nodes/DynamicAction.hpp>
#include <grunk/compute_nodes/Expression.hpp>
#include <grunk/Recipe.hpp>

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

ToStringVisitor::ToStringVisitor(YAML::Node& r) : root(r) {}

void ToStringVisitor::set_start_node(parametric::DAGNode const& n)
{
    start_node = &n;
}

void ToStringVisitor::visit(parametric::DAGNode const& n, size_t depth)
{
    // check if the node has already been parsed...
    if (visited(&n)) {
        return;
    }
    m_visited[&n] = true;

    // we visit children only if this is not the start node, or if 
    // no start node was specified.
    if ( (start_node && start_node != &n) || !start_node ) {
        // make sure that we have visited all direct children
        // for topological order of compute nodes
        n.accept(
                *this,
                depth,
                parametric::DAGNode::Direction::down
        );
    }

    bool is_compute_node = ((depth %  2) == 1);
    bool is_root_parameter = (n.num_parents() == 0) && !is_compute_node;

    if (!is_compute_node && n.id() != "") {
        feature_names_count[n.id()]++;
    }

    if (is_root_parameter || is_compute_node){

        auto node =  YAML::Load(n.serialize());

        if (is_root_parameter) {

            if (n.id() != "") {
                // constants are unnamed
                if (root["parameters"][n.id()]) {
                    throw io_error(
                        "The feature tree does not have unique feature names. Found duplicate parameter \""
                        + n.id() + "\"."
                    );
                }

                node.SetStyle(YAML::EmitterStyle::Flow);
                root["parameters"][n.id()] = node;
            }
        }
        else {
            // is action

            if (auto const* recipe_action_ptr = dynamic_cast<Recipe::Action const*>(&n); recipe_action_ptr) {
                if (!recipes[recipe_action_ptr->name()]) {
                    recipes[recipe_action_ptr->name()] 
                        = recipe_action_ptr->get_recipe()->serialize();
                }
            }

            steps.push(node);
        }
    }

}

void ToStringVisitor::unwind_steps() 
{
    while (!steps.empty()) {
        root["steps"].push_back(steps.top());
        steps.pop();
    }
    if (recipes && recipes.size() > 0) {
        root["recipes"] = recipes;
    }
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

YAML::Node serialize(parametric::DAGNode const& node)
{
    bool is_constant = (node.num_parents() == 0 && node.id() == "");
    if (is_constant) {
        return YAML::Load(node.serialize());
    } else {
        return YAML::Node(node.id());
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
