// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include <map>

namespace grunk {

namespace details {

    class ToStringVisitor;

    /**
     * @brief Converts a constructor syntax string to a new feature syntax string.
     *
     * This function checks if the input string contains ".new", which indicates that it is a userdata type. If it does, it modifies the string to use ".new_feature" instead. If it does not, it assumes the value is a primitive and wraps it with "grunk.feature()".
     *
     * @param s The input constructor syntax string.
     * @return A string in the new feature syntax format.
     */
    inline std::string ctor_syntax_to_new_feature_syntax(std::string const& s) {
        // we need to check if the value is userdata or a primitive. userdata contains .new somewhere in the 
        // serialized string.
        size_t pos = s.find(".new");
        if (pos == std::string::npos) {
            // the value is a primitive. We create it with grunk.feature
            if (s == "nil") {
                return "grunk.feature()";
            }
            return "grunk.feature(" + s + ")";
        } else {
            // its a usertype. Instead of Foo.new(xxx) we serialize Foo.new_feature(xxx)
            std::string s_cpy = s;
            s_cpy.insert(pos + 4, "_feature");
            return s_cpy;
        }
    }
} // namespace details

/**
 * @brief The Serializer class is responsible for converting a feature DAG into a YAML string representation. It traverses the DAG in a depth-first manner, visiting each node and serializing it according to its type (parameter or compute node). The resulting YAML string can be used to save recipes to file or to display them in a human-readable format.
 *
 * @ingroup advanced_dynamic
 */
class Serializer
{
public:
    /// @brief A typedef for an unordered map of boolean flags for every node. This is used to keep track of the nodes that have already been visited
    using Visited = std::unordered_map<parametric::DAGNode const*, bool>;
    friend class details::ToStringVisitor;

    /**
     * @brief Construct a new Serializer object
     */
    Serializer() = default;

    /**
     * @brief Get the parameters of the serialized feature tree. This is a map from parameter names to their serialized values. The parameters are the root nodes of the feature tree that have no parents and are not compute nodes. They are listed in the "parameters" block of the YAML representation of the recipe.
     *
     * @return std::map<std::string, std::string> const& A map from parameter names to their serialized values.
     */
    inline std::map<std::string, std::string> const& get_parameters() const {
        return parameters;
    }

    /**
     * @brief Get the steps of the serialized feature tree. This is a vector of strings, where each string is a serialized compute node in the feature tree. The steps are the compute nodes of the feature tree that are not root nodes and are not placeholders. They are listed in the "steps" block of the YAML representation of the recipe.
     *
     * @return std::vector<std::string> A vector of strings, where each string is a serialized compute node in the feature tree.
     */
    inline std::vector<std::string> get_steps() const {
        std::vector<std::string> ret;
        std::stack<std::string> temp_steps = steps;
        while (!temp_steps.empty()) {
            ret.push_back(temp_steps.top());
            temp_steps.pop();
        }
        return ret;
    }

    /**
     * @brief Get the string representation of the serialized feature tree. This is a single string that contains the serialized compute nodes in the feature tree, separated by newlines. The string can be used as the value of the "steps" block in the YAML representation of the recipe. If with_root_nodes is true, the string also includes the root nodes of the feature tree as parameter assignments at the beginning of the string.
     *
     * @param with_root_nodes If true, include the root nodes of the feature tree as parameter assignments at the beginning of the string.
     * @return std::string A string representation of the serialized feature tree.
     */
    inline std::string get_string(bool with_root_nodes = false) const {
        std::vector<std::string> step_list = get_steps();
        std::string ret = "";
        if (with_root_nodes) {
            for (auto const& [key, value] : parameters) {
                auto val = details::ctor_syntax_to_new_feature_syntax(value);
                ret += key + " = " + val + "\n";
            }
        }
        bool first = true;
        for (auto const& step : step_list) {
            if (!first) {
                ret += "\n";
            } else {
                first = false;
            }
            ret += step;
        }
        return ret;
    }

    /**
     * @brief Parses a feature and its underlying DAG into the Serializer. This function takes a feature as input and traverses its DAG in a depth-first manner, visiting each node and serializing it according to its type (parameter or compute node). The resulting serialized nodes are stored in the "parameters" and "steps" members of the Serializer, which can be accessed using the corresponding getter functions.
     *
     * @tparam Arg The type of the feature value, e.g. double, std::string or even a user defined type.
     * @param f The feature to be parsed into the Serializer.
     */
    template <typename Arg>
    void parse(Feature<Arg> const& f);

private:

    std::unordered_map<std::string, int> feature_names_count;
    Visited m_visited;

    std::map<std::string, std::string> parameters;
    std::stack<std::string> steps;
};

namespace details {

/**
 * @brief Trims leading and trailing whitespace from a string. This function is used to clean up the serialized strings of compute nodes and parameters before storing them in the Serializer. It removes any whitespace characters (space, tab, newline, carriage return, form feed, vertical tab) from the beginning and end of the input string.
 *
 * @param s The string to be trimmed. The function modifies the input string in place.
 * @ingroup advanced_dynamic
 */
inline void trim(std::string& s)
{
    const char* whitespace = " \t\n\r\f\v";
    s.erase(0, s.find_first_not_of(whitespace));
    s.erase(s.find_last_not_of(whitespace) + 1);
}

/**
 * @brief The ToStringVisitor class is a visitor for traversing the DAG of a feature and serializing its nodes into a YAML string representation. It is used internally by the Serializer class to convert a feature DAG into a YAML string. The visitor uses a depth-first traversal of the DAG, starting from the given node and visiting its children recursively. It keeps track of visited nodes to avoid redundant work and to ensure that each node is serialized only once.
 *
 * @ingroup advanced_dynamic
 */
class ToStringVisitor
{
public:

    /**
    * @brief Construct a new ToStringVisitor object
    */
    inline ToStringVisitor(Serializer& tree, parametric::DAGNode const& start_node)
     : start_node(start_node)
     , tree(tree)
    {}

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
    inline void visit(parametric::DAGNode const& n, size_t depth)
    {
        // check if the node has already been parsed...
        if (visited(&n)) {
            return;
        }
        tree.m_visited[&n] = true;

        // we visit children only if this is not the start node, or if 
        // no start node was specified.
        if ( &start_node != &n) {
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
            if (++tree.feature_names_count[n.id()] > 1) {
                throw io_error(
                    "The feature tree does not have unique feature names. Found duplicate feature name \""
                    + n.id() + "\"."
                );
            }
        }

        if (is_root_parameter || is_compute_node){

            std::string node;
            if (is_root_parameter) {
                n.eval();
            }
            bool is_placeholder = (is_root_parameter && !n.IsValid()); // an invalid root node is a placeholder
            if (is_placeholder) {
                node = "nil";
            } else {
                node = n.serialize();
            }

            if (is_root_parameter) {
                if (n.id() != "") {
                    // a parameter node
                    tree.parameters[n.id()] = node;
                }
            }
            else {
                // is action

                // check if this is an anonymous compute node (all children aka outputs are unnamed)
                bool is_anonymous = true;
                for (auto const& child_weak_ptr : n.get_children()) {
                    if (auto child_ptr = child_weak_ptr.lock(); child_ptr) {
                        if (child_ptr->id() != "") {
                            is_anonymous = false;
                            break;
                        }
                    }
                }
                if (!is_anonymous) {
                   tree.steps.push(node);
                }
            }
        }
    }

private:

    inline bool visited(parametric::DAGNode const* key) {
        return (tree.m_visited.find(key) != tree.m_visited.end());
    }

    parametric::DAGNode const& start_node;
    Serializer& tree;
};

} // namespace details

template <typename Arg>
inline void Serializer::parse(Feature<Arg> const& f)
{
    auto const& node = *f.node_pointer();
    details::ToStringVisitor visitor(*this, node);
    node.accept(visitor, 0, parametric::DAGNode::Direction::up);
}

} // namespace grunk
