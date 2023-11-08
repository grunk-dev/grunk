#include <grunk/compute_nodes/DynamicVec.hpp>

namespace grunk {

DynamicFeature vec(std::string const& id, std::vector<DynamicFeature> const& args)
{
    auto ret = parametric::compute(std::shared_ptr<DynamicVec>(new DynamicVec{}), args);
    ret.set_id(id);
    return DynamicFeature(std::move(ret), reflect::resolve<std::vector<reflect::DynamicObject>>());
}

} // namespace grunk
