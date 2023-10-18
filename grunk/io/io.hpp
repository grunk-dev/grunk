/**
 * @file io.hpp
 * 
 * This file contains all routines needed for reading a feature tree
 * from a yaml file and writing a feature tree to a yaml file
 * 
 * @defgroup fileio
 * 
 */

#pragma once 

#include <grunk/io/io_error.hpp>
#include <grunk/parametric_core.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/plugins/PluginRegistry.hpp>
#include <grunk/version.hpp>

#include <utility>
#include <initializer_list>
#include <stack>
#include <fstream>


namespace grunk {

    class Recipe;

namespace details {

/**
 * @brief parse_feature uses a Visitor pattern to recursively parse all dependent nodes.
 * While doing so, it must remember which nodes it has already visited and which nodes it 
 * hasn't. For this purpose, it stores a collection of type Visited.
 */
using Visited = std::unordered_map<parametric::DAGNode const*, bool>;

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
void parse_feature(Feature<Arg> const& arg, YAML::Node& yaml_root, Visited& visited, std::unordered_map<std::string, int>& feature_names)
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
        ToStringVisitor(YAML::Node& r, Visited& v, std::unordered_map<std::string, int>& feature_names)
         : root(r)
         , m_visited(v)
         , m_names(feature_names)
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

            bool is_compute_node = ((depth %  2) == 1);
            bool is_root_parameter = (n.num_parents() == 0) && !is_compute_node;

            if (!is_compute_node) {
                m_names[n.id()]++;
            }

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
        std::unordered_map<std::string, int>& m_names;
        std::stack<YAML::Node> steps;
        YAML::Node& root;
    };
    
    ToStringVisitor visitor(yaml_root, visited, feature_names);
    
    auto const& node = *(arg.param().node_pointer());
    node.accept(
        visitor,
        0,
        parametric::DAGNode::Direction::up
    );
    visitor.unwind_steps();
}

} //namespace details

std::string to_string(Recipe const&);

/**
 * @brief writes a ::grunk::Recipe to file
 * 
 * @param filename the filename of the output file
 * @param recipe the ::grunk::Recipe to be serialized to yaml
 */
void write(std::string const& filename, Recipe const& recipe);

template <typename... Args>
YAML::Node serialize(Feature<Args> const&... args);

/**
 * @brief writes several ::grunk::Feature instances to string
 * 
 * @tparam Args the types of the features
 * @param args the features
 * @return std::string  the serialized grunk recipe
 */
template <typename... Args>
std::string to_string(Feature<Args> const&... args)
{
    YAML::Emitter out;
    out << serialize(args...);
    return out.c_str();
}

/**
 * @brief writes several ::grunk::Feature instances to string
 * 
 * @tparam Args the types of the features
 * @param filename the filename of the output file
 * @param args the features
 */
template <typename... Args>
void write(std::string const& filename, Feature<Args> const&... args)
{
    std::ofstream fout(filename);
    fout << to_string(args...) << "\n";
}

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

} // namespace details

/**
 * @brief reads a recipe from a yaml file. 
 * 
 * @param filename The file to be parsed
 * @return Recipe the resulting recipe
 *
 * @ingroup fileio
 */
Recipe read(std::string filename);

} //namespace grunk
