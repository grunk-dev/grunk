#include "RuntimeAlgorithm.h"

namespace grunk {

RuntimeAlgorithmPtr eval(std::string const& id, Reflect::DynamicFunction const& fun, std::initializer_list<Feature<Reflect::DynamicObject>> const& args)
{
    return details::RuntimeAlgorithmFactory::new_algorithm(id, fun, args);
}

} //namespace grunk
