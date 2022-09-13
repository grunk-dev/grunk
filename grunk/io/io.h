#pragma once 

#include <grunk/core/Feature.h>

#include <yaml-cpp/yaml.h>
#include <initializer_list>
#include <stack>

namespace parametric {

template <>
std::string serialize(double const& v) {
    return YAML::Node(v).as<std::string>();
}

template <>
std::string serialize(int const& v) {
    return YAML::Node(v).as<std::string>();
}

template <>
std::string serialize(std::string const& v) {
    return YAML::Node(v).as<std::string>();
}

template <>
std::string serialize(Reflect::DynamicObject const& v)
{
    return Reflect::cast<std::string>(v.invoke("serialize")[0]);
}

} // namespace parametric

namespace grunk {

namespace details {

using Visited = std::unordered_map<parametric::DAGNode const*, bool>;

template <typename Arg>
void parse_feature(Feature<Arg> const& arg, YAML::Node& yaml_root, Visited& visited)
{

    class ToStringVisitor
    {
    public:
        ToStringVisitor(YAML::Node& r, Visited& v)
         : root(r)
         , visited(v)
        {};

        void visit(parametric::DAGNode const& n, size_t depth)
        {
            // check if the node has already been parsed...
            if (visited[&n]) {
                return;
            }

            if (std::string str = n.serialize(); !str.empty()){

                auto node =  YAML::Load(str);

                bool is_parameter = ((depth % 2) == 0);

                if (is_parameter) {
                    root["parameters"][n.id()] = node;
                }
                else {
                    // is algorithm
                    steps.push(node);
                }
            }

            visited[&n] = true;
        }

        // unwinding the steps makes sure that we write the steps in topological order
        void unwind_steps() {
            while (!steps.empty()) {
                root["steps"].push_back(steps.top());
                steps.pop();
            }
        }

    private:
        Visited& visited;
        std::stack<YAML::Node> steps;
        YAML::Node& root;
    };
    
    ToStringVisitor visitor(yaml_root, visited);
    
    auto const& node = *(arg.param().node_pointer());
    node.accept(
        visitor,
        0,
        parametric::DAGNode::Direction::up
    );
    visitor.unwind_steps();
}

template <typename... Args>
YAML::Node parse_feature_tree(Feature<Args> const&... args)
{
    YAML::Node root;
    Visited visited;

    //TODO: Required plugins
    root["uses"]["grunk"] = "0.2.16";
    root["uses"]["pluginA"] = "0.1.1";
    
    ([&](auto const& node){
        parse_feature(node, root, visited);
    }(args), ...);

    return root;
}

} //namespace details

template <typename... Args>
std::string serialize(Feature<Args> const&... args)
{
    YAML::Emitter out;
    out << details::parse_feature_tree(args...);
    return out.c_str();
}

} //namespace grunk