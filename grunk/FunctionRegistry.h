#pragma once

#include <initializer_list>
#include <unordered_map>

#include "Algorithm.h"

namespace grunk {

class FunctionRegistry
{
public:

    struct Entry {

        template <typename F>
        Entry(F const& algo_factory, std::string doc = "")
         : factory(algo_factory)
         , documentation(doc)
        {}

        std::function<parametric::compute_node_ptr<RuntimeAlgorithm>(std::initializer_list<Feature<RuntimeObject>> const&)> factory;
        std::string documentation;
    };

    FunctionRegistry() = default;

    void insert(std::string const&, Entry const&);

    Entry const& operator[](std::string const&) const;

    void clear();

private:

    std::unordered_map<std::string, Entry> algorithmMap;

};

// lazy creation of static algorithm registry
FunctionRegistry& get_function_registry();

template <typename F>
void register_function(F&& f,
                      std::string const& name,
                      std::string doc = "")
{
    RuntimeFunction<F> func(f);
    auto& registry = get_function_registry();

    registry.insert(
        name,
        {
            [=](auto& args) {
                return eval(func, args);
            },
            doc
        }
    );
}

template <typename... Args>
RuntimeAlgorithmPtr eval(std::string const& name, Feature<Args> const&... args)
{
    return get_function_registry()[name].factory({args...});
}

} //namespace grunk
