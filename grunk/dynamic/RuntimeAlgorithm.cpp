#include "RuntimeAlgorithm.h"

namespace grunk {

RuntimeAlgorithmPtr eval(std::string const& id, Reflect::DynamicFunction const& fun, std::vector<RuntimeFeature> const& args)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(id, fun, args);
}

RuntimeAlgorithmPtr eval(std::string const& id, std::string const& name, std::vector<RuntimeFeature> const& args)
{
    auto const& f = Reflect::GetFunctionRegistry().Resolve(name);
    return eval(id, f, args);
}

} //namespace grunk
