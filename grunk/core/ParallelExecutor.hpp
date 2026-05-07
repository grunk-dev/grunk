// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#ifdef GRUNK_WITH_TASKFLOW
#include <taskflow/taskflow.hpp>
#include "grunk/core/Feature.hpp"

#ifdef GRUNK_WITH_RECIPE
#include "grunk/core/parametric_core.hpp"
#include "grunk/recipe/Recipe.hpp"
#endif

namespace grunk {

namespace details {

    /**
     * @brief The TaskflowVisitor class template is a visitor for traversing the DAG of compute nodes in a feature tree and building a taskflow from it. It is used internally by the to_taskflow function to convert a vector of feature nodes into a taskflow. The visitor uses a depth-first traversal of the DAG, starting from the given nodes and visiting their parents and children recursively. It keeps track of visited nodes and created tasks to avoid redundant work and to establish the correct dependencies between tasks.
     *
     * @tparam Flow the type of the taskflow to build, either tf::Taskflow or tf::Subflow
     *
     * @ingroup advanced_core
     */
    template <typename Flow>
    class TaskflowVisitor
    {
        static_assert(
            std::is_same_v<Flow, tf::Taskflow> ||
            std::is_same_v<Flow, tf::Subflow>,
            "TaskflowVisitor: Flow must be either tf::Taskflow or tf::Subflow"
        );

        using Visited = std::unordered_map<parametric::DAGNode const*, bool>;
        using Tasks = std::unordered_map<parametric::DAGNode const*, tf::Task>;

    public:

        /**
         * @brief constructs a TaskflowVisitor with the given taskflow to build. The taskflow can be either a tf::Taskflow or a tf::Subflow, depending on the context of the traversal. The visitor will add tasks to this taskflow and establish dependencies between them based on the structure of the DAG.
         *
         * @param flow the taskflow to build
         */
        TaskflowVisitor(Flow& flow)
         : flow{flow}
         , first{true}
        {}

        /**
         * @brief visits a DAG node and builds the corresponding task in the taskflow. The visitor checks if the node has already been visited to avoid redundant work. If the node is a compute node (i.e. it is at an even depth in the DAG), it creates a task for it if it does not already exist, and establishes dependencies between the task of the node and the tasks of its parents and children. The visitor assumes that each compute node has at most one parent, which is the compute node that computes it. The visitor also gives the tasks names based on the ids of the nodes for better readability.
         *
         * @param n the DAG node to visit
         * @param depth the depth of the node in the DAG, used to determine if it is a compute node or a feature node
         */
        inline void visit(parametric::DAGNode const& n, size_t depth)
        {
            // check if the node has already been parsed...
            if (visited(&n)) {
                return;
            }
            m_visited[&n] = true;

            bool is_feature = ((depth %  2) == 0);
            if (!is_feature){
                return;
            }

            // I can have at most one parent: The compute node that computes me
            assert(n.get_parents().size() <= 1);

            if (n.get_parents().size() == 0) {
                // I am a root node. Nothing to be done.
                return;
            }

            //  parent_compute_node is the compute_node that computes me
            auto const& parent = n.get_parents()[0];

            // emplace the parent compute node, if not already done
            if (!has_task(parent.get())) {
                m_tasks[parent.get()] = flow.emplace([parent]() {
                    parent->eval();
                }).name(n.id().empty() ? "<anonymous>" : n.id());
            }

            if (first) {
                first = false;
                return;
            }

            for (auto& child : n.get_children()) {

                // my children are the compute nodes that use me
                if(auto c = child.lock(); c){
                    // emplace the child compute node, if not already done
                    if (!has_task(c.get())) {
                        m_tasks[c.get()] = flow.emplace([c]() {
                            c->eval();
                        }).name("<anonymous>");
                        // give the task the name of the childs output feature, if any
                        if (c->get_children().size() > 0) {
                            auto grandchild = c->get_children()[0].lock();
                            if (grandchild) {
                                m_tasks[c.get()].name(grandchild->id().empty() ? "<anonymous>" : grandchild->id());
                            }
                        }
                    }

                    // add dependency: child depends on parent
                    m_tasks[c.get()].succeed(m_tasks[parent.get()]);

                }
            }
        }

    private:

        /**
         * @brief checks if a DAG node has already been visited by the visitor. This is used to avoid redundant work and infinite recursion in case of cyclic dependencies in the DAG. The visitor maintains a map of visited nodes, where the key is a pointer to the DAG node and the value is a boolean indicating whether the node has been visited or not.
         *
         * @param key the DAG node to check
         * @return true if the node has already been visited, false otherwise
         */
        inline bool visited(parametric::DAGNode const* key) {
            return (m_visited.find(key) != m_visited.end());
        }

        /**
         * @brief checks if a DAG node has already been added as a task to the taskflow. This is used to avoid redundant work and to establish the correct dependencies between tasks. The visitor maintains a map of created tasks, where the key is a pointer to the DAG node and the value is the corresponding task in the taskflow.
         *
         * @param key the DAG node to check
         * @return true if the node has already been added as a task, false otherwise
         */
        inline bool has_task(parametric::DAGNode const* key) {
            return (m_tasks.find(key) != m_tasks.end());
        }

        Flow& flow;
        bool first;
        Visited m_visited;
        Tasks m_tasks;
    };

    /**
     * @brief converts a vector of feature nodes into a taskflow by traversing the DAG of compute nodes and building tasks for them. The function creates a TaskflowVisitor and uses it to visit each node in the input vector, starting from the given nodes and visiting their parents and children recursively. The resulting taskflow contains tasks for all compute nodes that are reachable from the input nodes, with dependencies established based on the structure of the DAG.
     *
     * @param nodes the vector of feature nodes to convert into a taskflow
     * @return tf::Taskflow the resulting taskflow built from the input nodes
     *
     * @ingroup core
     */
    inline tf::Taskflow to_taskflow(std::vector<parametric::NodeRef> const& nodes)
    {
        tf::Taskflow taskflow;
        TaskflowVisitor visitor(taskflow);
        for (auto const& node: nodes) {
            if (node == nullptr) {
                throw std::runtime_error("ParallelExecutor: Cannot build taskflow for null node.");
            }
            node->accept(visitor, 0, parametric::DAGNode::Direction::up);
        }
        return taskflow;
    }

}

/**
 * @brief converts a variadic list of features into a taskflow by traversing the DAG of compute nodes and building tasks for them. The function creates a TaskflowVisitor and uses it to visit each node corresponding to the input features, starting from the given nodes and visiting their parents and children recursively. The resulting taskflow contains tasks for all compute nodes that are reachable from the input nodes, with dependencies established based on the structure of the DAG.
 *
 * @tparam T the types of the input features
 * @param feature the variadic list of input features to convert into a taskflow
 * @return tf::Taskflow the resulting taskflow built from the input features
 *
 * @ingroup core
 */
template <typename... T>
tf::Taskflow to_taskflow(Feature<T> const&... feature)
{
    // parallel execution is only supported in dynamic mode, i.e. no T is a sol::object
    static_assert(
        (... && !std::is_same_v<T, sol::object>),
        "Parallel execution is only supported for non-dynamic features." 
    );
    return details::to_taskflow({feature.node_pointer()...});
}


/**
 * @brief The ParallelExecutor class is a utility for executing a set of features in parallel using Taskflow. It takes a variadic list of features as input, builds a taskflow from their corresponding DAG nodes, and runs the taskflow using a Taskflow executor. The ParallelExecutor can be constructed with a specified number of threads for the executor, and it provides methods to build the taskflow, run it, and manage its state.
 *
 * @ingroup core
 */
class ParallelExecutor 
{
public:

    /**
     * @brief constructs a ParallelExecutor with the given features. The constructor takes a variadic list of features as input, extracts their corresponding DAG nodes, and initializes the internal state of the executor. The constructor also checks that all input features are non-dynamic (i.e. they do not have sol::object as their type), since parallel execution is only supported for non-dynamic features.
     *
     * @tparam T the types of the input features
     * @param feature the variadic list of input features to execute in parallel
     *
     */
    template <typename... T>
    ParallelExecutor(Feature<T> const&... feature)
     : nodes{feature.node_pointer()...}
     , taskflow()
     , executor()
    {
        // parallel execution is only supported in dynamic mode, i.e. no T is a sol::object
        static_assert(
            (... && !std::is_same_v<T, sol::object>),
            "Parallel execution is only supported for non-dynamic features." 
        );
    }

    /**
     * @brief constructs a ParallelExecutor with the given features and number of threads. The constructor takes a variadic list of features as input, extracts their corresponding DAG nodes, and initializes the internal state of the executor with the specified number of threads. The constructor also checks that all input features are non-dynamic (i.e. they do not have sol::object as their type), since parallel execution is only supported for non-dynamic features.
     *
     * @tparam T the types of the input features
     * @param num_threads the number of threads to use for the Taskflow executor
     * @param feature the variadic list of input features to execute in parallel
     *
     */
    template <typename... T>
    ParallelExecutor(size_t num_threads, Feature<T> const&... feature)
     : nodes{feature.node_pointer()...}
     , taskflow()
     , executor(num_threads)
    {
        // parallel execution is only supported in dynamic mode, i.e. no T is a sol::object
        static_assert(
            (... && !std::is_same_v<T, sol::object>),
            "Parallel execution is only supported for non-dynamic features." 
        );
    }

    /**
     * @brief builds the taskflow from the input features by traversing the DAG of compute nodes and building tasks for them. The function creates a TaskflowVisitor and uses it to visit each node corresponding to the input features, starting from the given nodes and visiting their parents and children recursively. The resulting taskflow contains tasks for all compute nodes that are reachable from the input nodes, with dependencies established based on the structure of the DAG.
     *
     */
    inline void build_taskflow() {
        taskflow = details::to_taskflow(nodes);
    }

    /**
     * @brief returns a const reference to the internal taskflow of the executor. This allows users to inspect the structure of the taskflow, e.g. for debugging or visualization purposes, without modifying it. The taskflow is built from the input features and their corresponding DAG nodes, and it contains tasks for all compute nodes that are reachable from the input nodes, with dependencies established based on the structure of the DAG.
     *
     * @return tf::Taskflow const& a const reference to the internal taskflow of the executor
     *
     */
    inline tf::Taskflow const& get_taskflow() const {
        return taskflow;
    }

    /**
     * @brief resets the internal state of the executor by clearing the taskflow. This allows users to reuse the same executor instance for different sets of features or to rebuild the taskflow from the same features after modifying their DAG structure. The nodes vector is not modified by this function, so it still contains the DAG nodes corresponding to the input features, but the taskflow is cleared and needs to be rebuilt before running again.
     *
     */
    inline void reset() {
        taskflow.clear();
    }

    /**
     * @brief sets the name of the taskflow for better readability and debugging. The name is used in the Taskflow visualization and can help users to identify the purpose of the taskflow, e.g. by giving it a descriptive name based on the input features or the context of the execution.
     *
     * @param name the name to set for the taskflow
     */
    inline void name(std::string const& name) {
        taskflow.name(name);
    }

    /**
     * @brief returns a const reference to the name of the taskflow. This allows users to inspect the name of the taskflow, e.g. for debugging or visualization purposes, without modifying it. The name is set by the user and can be used to identify the purpose of the taskflow, e.g. by giving it a descriptive name based on the input features or the context of the execution.
     *
     * @return std::string const& a const reference to the name of the taskflow
     *
     */
    inline std::string const& name() const {
        return taskflow.name();
    }

    /**
     * @brief runs the taskflow using the Taskflow executor. The function checks if the taskflow is empty, and if so, it builds the taskflow from the input features by traversing the DAG of compute nodes and building tasks for them. Then it runs the taskflow using the executor and waits for its completion. The execution of the taskflow will evaluate all compute nodes that are reachable from the input features, with dependencies established based on the structure of the DAG.
     *
     */
    inline void run() {
        if (taskflow.empty()) {
            build_taskflow();
        }
        executor.run(taskflow).wait();
    }

private:

    std::vector<parametric::NodeRef> nodes;
    tf::Taskflow taskflow;
    tf::Executor executor;
};

} // namespace grunk

#endif
