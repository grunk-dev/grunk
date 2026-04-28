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

        TaskflowVisitor(Flow& flow)
         : flow{flow}
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
        inline bool visited(parametric::DAGNode const* key) {
            return (m_visited.find(key) != m_visited.end());
        }

        inline bool has_task(parametric::DAGNode const* key) {
            return (m_tasks.find(key) != m_tasks.end());
        }

        Flow& flow;
        bool first;
        Visited m_visited;
        Tasks m_tasks;
    };

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

class ParallelExecutor 
{
public:

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

    inline void build_taskflow() {
        taskflow = details::to_taskflow(nodes);
    }

    inline tf::Taskflow const& get_taskflow() const {
        return taskflow;
    }

    inline void reset() {
        taskflow.clear();
    }

    inline void name(std::string const& name) {
        taskflow.name(name);
    }

    inline std::string const& name() const {
        return taskflow.name();
    }

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
