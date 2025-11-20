#pragma once 

#include <map>

namespace grunk {

namespace details {
    class ToStringVisitor;

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

class Serializer
{
public:
    using Visited = std::unordered_map<parametric::DAGNode const*, bool>;
    friend class details::ToStringVisitor;

    Serializer() = default;

    inline std::map<std::string, std::string> const& get_parameters() const {
        return parameters;
    }

    inline std::vector<std::string> get_steps() const {
        std::vector<std::string> ret;
        std::stack<std::string> temp_steps = steps;
        while (!temp_steps.empty()) {
            ret.push_back(temp_steps.top());
            temp_steps.pop();
        }
        return ret;
    }

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

    template <typename Arg>
    void parse(Feature<Arg> const& f);

private:

    std::unordered_map<std::string, int> feature_names_count;
    Visited m_visited;

    std::map<std::string, std::string> parameters;
    std::stack<std::string> steps;
};

namespace details {

inline void trim(std::string& s)
{
    const char* whitespace = " \t\n\r\f\v";
    s.erase(0, s.find_first_not_of(whitespace));
    s.erase(s.find_last_not_of(whitespace) + 1);
}

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
