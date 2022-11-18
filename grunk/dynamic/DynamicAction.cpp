#include "DynamicAction.hpp"

namespace grunk {

DynamicActionPtr eval(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args)
{
    return details::DynamicActionFactory::new_action(id, fun, args);
}

DynamicActionPtr eval(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args)
{
    auto const& f = reflect::get_function_registry().resolve(name);
    return eval(id, f, args);
}

} //namespace grunk
