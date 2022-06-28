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

FunctionRegistry& get_function_registry() {
    static FunctionRegistry registry;
    return registry;
}

} // namespace grunk