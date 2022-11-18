#include "RuntimeAction.hpp"

namespace grunk {

RuntimeActionPtr eval(std::string const& id, reflect::DynamicFunction const& fun, std::vector<RuntimeFeature> const& args)
{
    return details::RuntimeActionFactory::new_action(id, fun, args);
}

RuntimeActionPtr eval(std::string const& id, std::string const& name, std::vector<RuntimeFeature> const& args)
{
    auto const& f = reflect::get_function_registry().resolve(name);
    return eval(id, f, args);
}

} //namespace grunk
