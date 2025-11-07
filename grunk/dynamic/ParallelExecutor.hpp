#pragma once 

#ifdef GRUNK_WITH_TASKFLOW
#include <taskflow/taskflow.hpp>
#include "grunk/dynamic/feature.hpp"

#ifdef GRUNK_WITH_RECIPE
#include "grunk/dynamic/internal/parametric_core.hpp"
#include "grunk/recipe/Recipe.hpp"
#endif

namespace grunk {

namespace details {

#ifdef GRUNK_WITH_RECIPE
    void emplace_recipe(tf::Subflow& subflow, Recipe const& recipe);
#endif

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

#ifdef GRUNK_WITH_RECIPE
        if (auto const* recipe_ptr = dynamic_cast<parametric::impl::param_holder<grunk::Recipe> const*>(&n); recipe_ptr) {
            if (!has_task(&n)) {
                
                // get the taskflow of the recipe and run it asynchronously
                m_tasks[&n] = flow.emplace([recipe_ptr](tf::Subflow& subflow) {
                    subflow.retain(true); //TODO: Only for debugging This makes sure the subflow is retained for gaphviz visualization
                    auto const& recipe = recipe_ptr->value();
                    details::emplace_recipe(subflow, recipe);
                }).name("SubRecipeTask:" + n.id());

                // add dependency: async_recipe_task depends on parent
                m_tasks[&n].succeed(m_tasks[parent.get()]);

            }
        }
#endif

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
#ifdef GRUNK_WITH_RECIPE
                    if (has_task(&n)) {
                        // add dependency: child depends on async_recipe_task
                        m_tasks[c.get()].succeed(m_tasks[&n]);
                    }
                    else {
                        // add dependency: child depends on parent
                        m_tasks[c.get()].succeed(m_tasks[parent.get()]);
                    }
#else 
                    // add dependency: child depends on parent
                    m_tasks[c.get()].succeed(m_tasks[parent.get()]);
#endif
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

    inline void emplace_nodes(tf::Subflow& subflow, std::vector<parametric::NodeRef> const& nodes)
    {
        TaskflowVisitor visitor(subflow);
        for (auto const& node: nodes) {
            if (node == nullptr) {
                throw std::runtime_error("ParallelExecutor: Cannot build taskflow for null node.");
            }
            node->accept(visitor, 0, parametric::DAGNode::Direction::up);
        }
    }

}

template <typename... T>
tf::Taskflow to_taskflow(Feature<T> const&... feature)
{
    return details::to_taskflow({feature.node_pointer()...});
}

#ifdef GRUNK_WITH_RECIPE
namespace details {

inline void emplace_recipe(tf::Subflow& subflow, Recipe const& recipe)
{
    auto features = recipe.get_all_features();
    std::vector<parametric::NodeRef> nodes;

    nodes.reserve(features.size());
    std::transform(
        features.begin(), features.end(),
        std::back_inserter(nodes),  
        [](DynamicFeature const& f) {
            return f.node_pointer();
        }
    );
    emplace_nodes(subflow, nodes);
}

} // namespace details
#endif 

class ParallelExecutor 
{
public:

    template <typename... T>
    ParallelExecutor(Feature<T> const&... feature)
     : nodes{feature.node_pointer()...}
     , taskflow()
     , executor()
    {}

    template <typename... T>
    ParallelExecutor(size_t num_threads, Feature<T> const&... feature)
     : nodes{feature.node_pointer()...}
     , taskflow()
     , executor(num_threads)
    {}

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
