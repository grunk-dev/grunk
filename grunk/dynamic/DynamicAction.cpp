#include "DynamicAction.hpp"

namespace grunk {

DynamicActionPtr action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args)
{
    return details::DynamicActionFactory::new_action(id, fun, args);
}

DynamicActionPtr action(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args)
{
    auto const& f = reflect::resolve_function(name);
    return action(id, f, args);
}

} //namespace grunk
