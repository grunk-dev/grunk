#pragma once 

#include <grunk/core/Feature.h>

#include <yaml-cpp/yaml.h>
#include <initializer_list>
#include <stack>

namespace {

struct topo_sort {

    using Stack = std::stack<YAML::Node const*>;
    using Visited = std::unordered_map<std::string, bool>;

    Visited visited;
    YAML::Node const& steps;
    YAML::Node out;

    topo_sort(YAML::Node const& root_params, YAML::Node const& stps)
        : steps(stps)
    {
        // Mark all the vertices as not visited, except for the root nodes
        for(YAML::const_iterator it=root_params.begin();it!=root_params.end();++it) {
            visited[it->first.as<std::string>()] = true;
        }


        for (auto const& step : steps){
            if (!visited[step["outputs"][0].as<std::string>()]) {
                topo_sort_(step);
            }
        }

        // can this be improved? std::reverse doesn't work, because YAML iterators aren't
        // bidiretional
        YAML::Node tmp;
        for(int i = out.size()-1; i>=0; --i ){
            tmp.push_back(out[i]);
        }
        out = tmp;
    }

    void topo_sort_(YAML::Node const& step)
    {
        for (auto const& output : step["outputs"]) {
            visited[output.as<std::string>()] = true;
        }
        for (auto const& input: step["inputs"]) {
            if (!visited[input.as<std::string>()]){

                [&]{
                // find step producing input and recurse
                for (auto const& s : steps){
                    for (auto const& output : step["outputs"]){
                        if (output.as<std::string>() == input.as<std::string>()){
                            topo_sort_(s);
                            return; // we use a lambda and return so we can break out of two nested for-loops
                        }
                    }
                }
                }();
            }
        }
        out.push_back(step);
    }

    YAML::Node operator()() { return out; };
};

} // anonymous namespace

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
    return Reflect::cast<std::string>(v.invoke("serialize"));
}

} // namespace parametric

namespace grunk {

namespace details {

template <typename Arg>
void parse_feature(Feature<Arg> const& arg, YAML::Node& yaml_root)
{

    class ToStringVisitor
    {
    public:
        ToStringVisitor(YAML::Node& root)
         : yaml(root)
        {}

        void visit(parametric::DAGNode const& n, size_t depth)
        {

            // check if the node has already been parsed...

            // ... as a root parameter
            if (yaml["parameters"][n.id()]) {
                return;
            }

            // ... as an output of a calculation
            for( auto itr = yaml["steps"].begin(); itr != yaml["steps"].end() ; ++itr ) {
                for (auto const& output : (*itr)["outputs"]) {
                    if (output.as<std::string>() == n.id() ) {
                        return;
                    }
                }
            }

            if (std::string str = n.serialize(); !str.empty()){

                auto node =  YAML::Load(str);

                bool is_parameter = ((depth % 2) == 0);

                if (is_parameter) {
                    yaml["parameters"][n.id()] = node;
                }
                else {
                    // is algorithm
                    yaml["steps"].push_back(node);
                }
            }
        }

    private:
        YAML::Node& yaml;
    };
    
    ToStringVisitor visitor(yaml_root);
    
    auto const& node = *(arg.param().node_pointer());
    node.accept(
        visitor,
        0,
        parametric::DAGNode::Direction::up
    );
}

template <typename... Args>
YAML::Node parse_feature_tree(Feature<Args> const&... args)
{
    YAML::Node root;

    //TODO: Required plugins
    root["requires"]["grunk"] = "0.2.16";
    root["requires"]["pluginA"] = "0.1.1";
    
    ([&](auto const& node){
        parse_feature(node, root);
    }(args), ...);
    
    //TODO: topological sort for steps
    
    root["steps"] = topo_sort(root["parameters"], root["steps"])();

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