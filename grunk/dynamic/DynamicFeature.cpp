#include "DynamicFeature.hpp"
#include "DynamicAction.hpp"

namespace grunk {

DynamicFeature::Feature(parametric::param<reflect::DynamicObject>&& p, reflect::TypeDescriptor const* t)
    : FeatureBase<reflect::DynamicObject>(std::forward<parametric::param<reflect::DynamicObject>>(p))
    , type_descriptor(t)
{}

DynamicFeature::Feature(std::string const& id, reflect::DynamicObject&& o)
    : FeatureBase(id, std::forward<reflect::DynamicObject>(o))
    , type_descriptor(o.get_type_descriptor())
{}

DynamicFeature DynamicFeature::get(std::string const& id, std::string const& memberName) const
{
    return action(
        id, 
        type_descriptor->get_name() + "::" + memberName,
        *this
    ).output(0);
}

} //namespace grunk
