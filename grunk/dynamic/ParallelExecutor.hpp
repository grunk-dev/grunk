#pragma once 

#ifdef GRUNK_WITH_TASKFLOW
#include <taskflow/taskflow.hpp>
#include "grunk/dynamic/feature.hpp"

namespace grunk {

class ParallelExecutor 
{
public:

    template <typename T>
    ParallelExecutor(Feature<T> const& feature) {
        TaskflowVisitor(taskflow);
        auto const& node = *feature.node_pointer();
        node.accept(visitor, 0, parametric::DAGNODE::Direction::up);
    }

    inline void run() {
        exectur.run(taskflow).wait();
    }

private:

    tf::Executor executor;
    tf::Taskflow taskflow;

    class TaskflowVisitor
    {
        using Visited = std::unordered_map<parametric::DAGNode const*, bool>;
        using Tasks = std::unordered_map<parametric::DAGNode const*, tf::Taskflow>;

    public:

        TaskflowVisitor(tf::Taskflow& taskflow)
         : start_node(start_node)
         , taskflow(taskflow)
         , first{true}
        {}

        inline void visit(parametric::DAGNode const& n, size_t depth)
        {
            // check if the node has already been parsed...
            if (visited(&n)) {
                return;
            }
            tree.m_visited[&n] = true;

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

            // emplace the first compute node of the tree
            auto parent_task = taskflow.emplace([parent]() {
                parent->eval();
            });
            m_tasks[parent.get()] = parent_task;

            if (first) {
                first = false;
                return;
            }

            for (std::weak_ptr<DAGNode>& child : n.get_children()) {

                // my children are the compute nodes that use me
                if(auto c = child.lock(); c){
                    auto const& child = *c;
                    auto child_task = taskflow.emplace([c]() {
                        if (auto c = child.lock(); c) {
                            child->eval();
                        }
                    });
                    m_tasks[&child].succeed(parent_task);
                }
            }
        }

    private:
        inline bool visited(parametric::DAGNode const* key) {
            return (m_visited.find(key) != m_visited.end());
        }

        Visited m_visited;
        Tasks m_tasks;
        tf::Taskflow& taskflow;
        bool first;
    };
};

} // namespace grunk

#endif
