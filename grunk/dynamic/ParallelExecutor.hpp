#pragma once 

#ifdef GRUNK_WITH_TASKFLOW
#include <taskflow/taskflow.hpp>
#include "grunk/dynamic/feature.hpp"

namespace grunk {

class ParallelExecutor 
{
public:

    template <typename T>
    ParallelExecutor(Feature<T> const& feature)
     : node{feature.node_pointer()}
     , taskflow(feature.id().empty() ? "<anonymous>" : feature.id())
    {
    }

    void build_taskflow() {
        if (node == nullptr) {
            throw std::runtime_error("ParallelExecutor: Cannot build taskflow for null node.");
        }
        auto visitor = TaskflowVisitor(taskflow);
        node->accept(visitor, 0, parametric::DAGNode::Direction::up);
    }

    tf::Taskflow const& get_taskflow() const {
        return taskflow;
    }

    inline void reset() {
        taskflow.clear();
    }

    inline void run() {
        if (taskflow.empty()) {
            build_taskflow();
        }
        executor.run(taskflow).wait();
    }

private:

    parametric::NodeRef node;
    tf::Taskflow taskflow;
    tf::Executor executor;

    class TaskflowVisitor
    {
        using Visited = std::unordered_map<parametric::DAGNode const*, bool>;
        using Tasks = std::unordered_map<parametric::DAGNode const*, tf::Task>;

    public:

        TaskflowVisitor(tf::Taskflow& tf)
         : taskflow{tf}
         , first{true}
        {}

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

            // emplace the parent compute node
            auto parent_task = taskflow.emplace([parent]() {
                parent->eval();
            }).name(n.id().empty() ? "<anonymous>" : n.id());
            m_tasks[parent.get()] = parent_task;

            if (first) {
                first = false;
                return;
            }

            for (auto& child : n.get_children()) {

                // my children are the compute nodes that use me
                if(auto c = child.lock(); c){
                    m_tasks[c.get()].succeed(parent_task);
                }
            }
        }

    private:
        inline bool visited(parametric::DAGNode const* key) {
            return (m_visited.find(key) != m_visited.end());
        }

        tf::Taskflow& taskflow;
        bool first;
        Visited m_visited;
        Tasks m_tasks;
    };
};

} // namespace grunk

#endif
