#include <grunk/DynamicFeature.hpp>
#include <grunk/action.hpp>

namespace grunk {

DynamicFeature::Feature(param<reflect::DynamicObject>&& p, reflect::TypeDescriptor const* t)
    : FeatureBase<reflect::DynamicObject>(std::forward<param<reflect::DynamicObject>>(p))
    , type_descriptor(t)
{}

DynamicFeature::Feature(std::string const& id, reflect::DynamicObject&& o)
    : FeatureBase(id, std::forward<reflect::DynamicObject>(o))
    , type_descriptor(o.get_type_descriptor())
{}

} //namespace grunk
