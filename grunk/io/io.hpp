/**
 * @file io.h
 * 
 * This file contains all routines needed for reading a feature tree
 * from a yaml file and writing a feature tree to a yaml file
 * 
 * @defgroup fileio
 * 
 */

#pragma once 

#include <grunk/parametric_core.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/plugins/PluginRegistry.hpp>
#include <grunk/version.hpp>

#include <stdexcept>
#include <utility>
#include <initializer_list>
#include <stack>
#include <fstream>


namespace grunk {

using namespace std::string_literals;

/**
 * @brief An exception representing errors with grunk's file I/O system
 */
class io_error : public std::exception
{
public: 
    /**
     * @brief Construct a new io error object from an error message
     * 
     * @param msg 
     */
    io_error(std::string const& msg);

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

/**
 * @brief parse_feature uses a Visitor pattern to recursively parse all dependent nodes.
 * While doing so, it must remember which nodes it has already visited and which nodes it 
 * hasn't. For this purpose, it stores a collection of type Visited.
 */
using Visited = std::unordered_map<parametric::DAGNode const*, bool>;

/**
 * @brief checks a YAML tree for duplicate feature names
 * 
 * @param root The root node of the YAML representation
 * @return true if the tree has unique feature names
 * @return false otherwise
 */
bool has_unique_feature_names(YAML::Node const& root);

/**
 * @brief parses a Feature<Arg> for any given type arg to yaml. While doing so
 * it parses recursively all ancestors of the feature, that is all parametric::ComputeNodes
 * and all parameters the input feature depends on.
 *
 * This is an internal function called by feature_tree_to_yaml, which calls this
 * function on more than one feature.
 * 
 * @tparam Arg The type stored in the Feature object
 * @param arg The input feature
 * @param yaml_root A YAML::Node representing the feature tree
 * @param visited The Visited structure to check, if a node has already been visited
 */
template <typename Arg>
void parse_feature(Feature<Arg> const& arg, YAML::Node& yaml_root, Visited& visited)
{

    /**
     * @brief This visitor will visit all ancesstors of arg to collect
     * the feature tree in the yaml representation stored in the YAML::Node yaml_root.
     */
    class ToStringVisitor
    {
    public:
        /**
        * @brief Construct a new ToStringVisitor object
        * 
        * @param r Reference to the root yaml node of the tree
        * @param v Visited instance to check, if a node as already
        *          been visited
        */
        ToStringVisitor(YAML::Node& r, Visited& v)
         : root(r)
         , m_visited(v)
        {};

        /**
         * @brief visit a node and serialize to yaml.
         *
         * Here it is assumed, that every other node is a ComputeNode.
         * These compute nodes will be stored on an intermediate stack,
         * so that unstacking it will result in a topological orderd list
         * of serialized compute nodes, which will be written into the 
         * "steps" block of the yaml node.
         * 
         * Parameter nodes will directly be added to the "parameters" block,
         * if they are independent root nodes
         * 
         * @param n The current node to be visited
         * @param depth The current depth of the DFS traversal
         */
        void visit(parametric::DAGNode const& n, size_t depth)
        {
            // check if the node has already been parsed...
            if (visited(&n)) {
                return;
            }

            bool is_root_parameter = (n.num_parents() == 0);
            bool is_compute_node = ((depth %  2) == 1);

            if (is_root_parameter || is_compute_node){

                auto node =  YAML::Load(n.serialize());

                if (is_root_parameter) {


                    if (root["parameters"][n.id()]) {
                        throw io_error(
                            "The feature tree does not have unique feature names. Found duplicate parameter \""
                            + n.id() + "\"."
                        );
                    }

                    node.SetStyle(YAML::EmitterStyle::Flow);
                    root["parameters"][n.id()] = node;
                }
                else {
                    // is action
                    steps.push(node);
                }
            }

            m_visited[&n] = true;
        }

        /**
         * @brief unwinding the steps maeks sure we write the steps in 
         * topological order. Without calling this function after the DFS traversal,
         * no "steps" will be added to the root yaml node.
         * 
         */
        void unwind_steps() {
            while (!steps.empty()) {
                root["steps"].push_back(steps.top());
                steps.pop();
            }
        }

    private:

        bool visited(parametric::DAGNode const* key) {
            return (m_visited.find(key) != m_visited.end());
        }

        Visited& m_visited;
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


// Variable template that checks if a type has begin() and end() member functions
// taken from the example at https://en.cppreference.com/w/cpp/types/void_t
template <typename, typename = void>
constexpr bool is_iterable{};
  
template <typename T>
constexpr bool is_iterable<
    T,
    std::void_t< decltype(std::declval<T>().begin()),
                 decltype(std::declval<T>().end())
    >
> = true;
                                
// An iterator trait those value_type is the value_type of the iterated container,
// supports even back_insert_iterator (where value_type is void)
template <typename T, typename = void>
struct iterator_trait
: std::iterator_traits<T> {};
                                  
template <typename T>
struct iterator_trait<T, std::void_t<typename T::container_type>>
: std::iterator_traits<typename T::container_type::iterator> {};

/**
 * @brief A check if a type is std::pair for SFINAE
 * 
 * @tparam typename 
 */
template<typename> constexpr bool is_pair_v = false;

template<typename First, typename Second>
constexpr bool is_pair_v<std::pair<First, Second>> = true;

/**
 * @brief Takes an interable container of Feature<T> and 
 * creates a yaml representation. The Container must have a 
 * begin and end iterator. Map-like containers with a key are
 * supported, the key is ignored
 * 
 * @tparam Container type of Container
 * @param features The container of Feature<T> instances
 * @return YAML::Node A yaml representation of the feature tree(s)
 */
template <
    typename Container,
    typename = std::enable_if_t<is_iterable<Container>>
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

    if (!has_unique_feature_names(root)) {
        throw io_error("The feature tree does not have unique feature names.");
    }

    // write loaded plugins
    auto const& registry = get_plugin_registry();
    for(auto const& [name, entry] : registry.plugins()){
        root["uses"][name] = entry.plugin->version();
    }

    return root;
}

/**
 * @brief takes Feature<T> instances and creates a yaml representation
 * of the feature tree(s)
 * 
 * @tparam Args types contained in the input Feature instances
 * @param args The input Feature<T> instances
 * @return YAML::Node yaml representation of the feature tree.
 */
template <typename... Args>
YAML::Node feature_tree_to_yaml(Feature<Args> const&... args)
{
    return feature_tree_to_yaml(std::initializer_list<DynamicFeature>{args...});
}

} //namespace details

/**
 * @brief given a list of ``Feature``s, this function serializes
 * these features together with all their ancestors to string, which 
 * is interpretable by a yaml interpreter.
 * 
 * @tparam Args the types stored in the input ``Feature``s
 * @param args The input ``Feature``s
 * @return std::string the output string, interpretable as yaml
 *
 * @ingroup fileio
 */
template <typename ... Args>
std::string to_string(Feature<Args> const&... args)
{
    YAML::Emitter out;
    out << details::feature_tree_to_yaml(args...);
    return out.c_str();
}

/**
 * @brief given a container of ``Feature``s, this function serializes
 * these features together with all their ancestors to string, which 
 * is interpretable by a yaml interpreter.
 * 
 * @tparam Container the type of the container. The container type
 *                   must be iterable. Both list-like containers and
 *                   map-like containers are supported. For the latter, 
 *                   the keys are ignored.
 * @param features The input ``Feature``s
 * @return std::string he output string, interpretable as yaml
 *
 * @ingroup fileio
 */
template <
    typename Container,
    typename = std::enable_if_t<details::is_iterable<Container>>
>
std::string to_string(Container const& features)
{
    YAML::Emitter out;
    out << details::feature_tree_to_yaml(features);
    return out.c_str();
}

/**
 * @brief given a list of ``Feature``s, this function serializes
 * these features together with all their ancestors to a yaml file.
 * 
 * @tparam Args Accepted are either ``Feature<T>``s, or a container 
 *              storing ``Feature<T>``s. In the latter case, 
 *              the container must be iterable.
 * @param filename the filename of the output file
 * @param args the features to be serialized to yaml.
 *
 * @ingroup fileio
 */
template <typename... Args>
void write(std::string const& filename, Args const&... args)
{
    std::ofstream fout(filename);
    fout << to_string(args...) << "\n";
}

/**
 * @brief When deserializing a feature tree from yaml, the features
 * of the feature tree will be contained in a FeatureContainer.
 *
 * It is a typedef for an unordered_map, where the keys are the ids
 * of the features.
 * 
 * @ingroup fileio
 */
using FeatureContainer = std::unordered_map<std::string, DynamicFeature>;

namespace details {

/**
 * @brief deserializes a yaml node to a DynamicObject
 * 
 * @param type_name The name of the type. This type must be registered
 *                  in the type registry and  a deserialize method must exist.
 * @param yaml_node The yaml node passed on to the deserialize method of 
 *                  the registered type with name type_name.
 * @return reflect::DynamicObject the deserialized instance.
 */
reflect::DynamicObject deserialize(
    std::string const& type_name,
    YAML::Node const & yaml_node
);

/**
 * @brief creates a feature tree in the form of a ``FeatureContainer``
 * from a yaml representation. grunk uses the yaml-cpp library for this.
 * 
 * @param root The YAML::Node
 * @return FeatureContainer The resulting feature tree.
 */
FeatureContainer yaml_to_feature_tree(YAML::Node const& root);

} // namespace details

/**
 * @brief creates a feature tree from a yaml file. 
 * 
 * @param filename The file to be parsed
 * @return FeatureContainer the resulting feature tree
 *
 * @ingroup fileio
 */
FeatureContainer read(std::string filename);

} //namespace grunk
