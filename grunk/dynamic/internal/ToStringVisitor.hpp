#pragma once 

namespace grunk {

namespace details {

void trim(std::string& s)
{
    const char* whitespace = " \t\n\r\f\v";
    s.erase(0, s.find_first_not_of(whitespace));
    s.erase(s.find_last_not_of(whitespace) + 1);
}

} // namespace details

class ToStringVisitor
{
public:
    using Visited = std::unordered_map<parametric::DAGNode const*, bool>;
    /**
    * @brief Construct a new ToStringVisitor object
    */
    ToStringVisitor() = default;

    /**
     * @brief Specify the start node. Dependent nodes of the start node
     * are ignored. Dependent nodes of other nodes are not ignored, because
     * they need to be visited in the DFS traversal to get the correct topological
     * order of compute nodes
     * 
     * @param n the start node
     */
    inline void set_start_node(parametric::DAGNode const& n)
    {
        start_node = &n;
    }

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
            if (++feature_names_count[n.id()] > 1) {
                throw io_error(
                    "The feature tree does not have unique feature names. Found duplicate feature name \""
                    + n.id() + "\"."
                );
            }
        }

        if (is_root_parameter || is_compute_node){

            std::string node =  n.serialize();

            if (is_root_parameter) {
                if (n.id() != "") {
                    // a parameter node
                    parameters[n.id()] = node;
                }
            }
            else {
                // is action
                steps.push(node);
            }
        }
    }

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
                ret += key + " = grunk.feature(" + value + ")\n";
            }
        }
        for (auto const& step : step_list) {
            ret += step + "\n";
        }
        return ret;
    }

private:

    std::unordered_map<std::string, int> feature_names_count;

    inline bool visited(parametric::DAGNode const* key) {
        return (m_visited.find(key) != m_visited.end());
    }

    parametric::DAGNode const* start_node;
    Visited m_visited;
    std::map<std::string, std::string> parameters;
    std::stack<std::string> steps;
};

} // namespace grunk