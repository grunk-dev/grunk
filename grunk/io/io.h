#pragma once 

#include <grunk/dynamic/RuntimeFeature.h>
#include <grunk/plugins/PluginRegistry.h>
#include <grunk/version.h>

#include <utility>
#include <yaml-cpp/yaml.h>
#include <initializer_list>
#include <stack>
#include <fstream>

#include <iostream>

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
    return v;
}

template <>
std::string serialize(Reflect::DynamicObject const& v)
{ 
    auto serialized = 
        Reflect::cast<YAML::Node>(v.invoke("serialize")[0]);

    YAML::Node out;
    out["type"] = v.get_type_descriptor()->GetName();
    out["value"] = serialized;
    return YAML::Dump(out);
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

template<class T>
using is_iterable_impl = std::void_t<
    std::enable_if_t<std::is_same_v<
        decltype(std::begin(std::declval<T&>())), // has begin()
        decltype(std::end(std::declval<T&>()))    // has end()
    >>,                                      // ... begin() and end() are the same type ...
    decltype(*begin(std::declval<T&>()))     // ... which can be dereferenced
>;

template<class T, class = void>
struct is_iterable : std::false_type {};

template<class T>
struct is_iterable<T, is_iterable_impl<T>> : std::true_type {};

template<class T>
constexpr bool is_iterable_v = is_iterable<T>::value;

template<typename> constexpr bool is_pair_v = false;

template<typename First, typename Second>
constexpr bool is_pair_v<std::pair<First, Second>> = true;

template <
    typename Container,
    typename = std::enable_if_t<details::is_iterable_v<Container>>
>
YAML::Node feature_tree_to_yaml(Container const& features)
{
    YAML::Node root;
    Visited visited;

    //write grunk version
    root["uses"]["grunk"] = grunk_VERSION;
    
    for (auto const& value: features){
        if constexpr (details::is_pair_v<typename Container::value_type>)
        {
            // container is map-like
            parse_feature(value.second, root, visited);
        }
        else 
        {
            // container is vector/list like
            parse_feature(value, root, visited);
        }
    }

    // write loaded plugins
    auto const& registry = get_plugin_registry();
    for(auto const& [name, entry] : registry.plugins()){
        root["uses"][name] = entry.plugin->version();
    }

    return root;
}

template <typename... Args>
YAML::Node feature_tree_to_yaml(Feature<Args> const&... args)
{
    return feature_tree_to_yaml(std::initializer_list<RuntimeFeature>{args...});
}

} //namespace details

template <typename ... Args>
std::string to_string(Feature<Args> const&... args)
{
    YAML::Emitter out;
    out << details::feature_tree_to_yaml(args...);
    return out.c_str();
}

template <
    typename Container,
    typename = std::enable_if_t<details::is_iterable_v<Container>>
>
std::string to_string(Container const& features)
{
    YAML::Emitter out;
    out << details::feature_tree_to_yaml(features);
    return out.c_str();
}

template <typename... Args>
void write(std::string const& filename, Args const&... args)
{
    std::ofstream fout(filename);
    fout << to_string(args...);
}

} //namespace grunk