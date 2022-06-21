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

        std::function<parametric::compute_node_ptr<Algorithm>(std::initializer_list<Feature> const&)> factory;
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
FunctionRegistry& GetFunctionRegistry();

template <typename F>
void RegisterFunction(std::string const& name, 
                      F&& f, 
                      std::string doc = "")
{
    RuntimeFunction<F> func(f);
    auto& registry = GetFunctionRegistry();

    registry.insert(
        name,
        {
            [=](std::initializer_list<Feature> const& args) {
                return new_algorithm(func, args);
            },
            doc
        }
    );
}

parametric::compute_node_ptr<Algorithm> Eval(std::string const&, std::initializer_list<Feature> const&);

} //namespace grunk