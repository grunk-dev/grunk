#include "FunctionRegistry.h"

namespace grunk {

void FunctionRegistry::clear()
{
    algorithmMap.clear();
}

void FunctionRegistry::insert(std::string const& key, FunctionRegistry::Entry const& val)
{
    algorithmMap.insert({key, val});
}

FunctionRegistry::Entry const& FunctionRegistry::operator[](std::string const& key) const
{
    return algorithmMap.at(key);
}

FunctionRegistry& GetFunctionRegistry() {
    static FunctionRegistry registry;
    return registry;
}

parametric::compute_node_ptr<Algorithm> Eval(std::string const& name, std::initializer_list<Feature> const& args)
{
    return GetFunctionRegistry()[name].factory(args);
}

} // namespace grunk