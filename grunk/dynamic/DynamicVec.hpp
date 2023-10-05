#pragma once 

#include <grunk/core/Feature.hpp>
#include <grunk/parametric_core.hpp>
#include <grunk/helper/common.hpp>
#include <grunk/core/Vec.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>

#include <vector>

namespace grunk {

template <>
class Vec<reflect::DynamicObject> : public parametric::ComputeNode<Vec<reflect::DynamicObject>, parametric::Results<reflect::DynamicObject>, details::ignore>
{
public:
    Vec() = default;
    void eval() const override final
    {
        std::vector<reflect::DynamicObject> v;
        v.reserve(this->parents.size());
        for (size_t i=0; i<this->parents.size(); ++i) {
            v.push_back(this->template arg<reflect::DynamicObject>(i).value());
        }
        if (auto r =  this->template res<0>(); r) {
            r->set_value(reflect::DynamicObject(std::move(v)));
        }
    }

private:
};

using DynamicVec = Vec<reflect::DynamicObject>;

namespace {
    // needed because of bug in MSVC 2017: Can't use a fold expression in std::enable_if_t of vec definition
    template <typename... Ts>
    constexpr bool is_dynamic_feature_v = std::is_same_v<std::tuple<DynamicFeature, Ts...>, std::tuple<Ts..., DynamicFeature>>;
}

template <typename... Args>
std::enable_if_t<is_dynamic_feature_v<Args...>, DynamicFeature> 
vec(std::string const& id, DynamicFeature const& f, Args const&... fs)
{
    static_assert((std::is_same_v<std::decay_t<Args>, DynamicFeature> && ...));

    auto ret = parametric::compute<DynamicVec>(f.param(), fs.param()...);
    ret.set_id(id);
    return DynamicFeature(std::move(ret), reflect::resolve<std::vector<reflect::DynamicObject>>());
}

} // namespace grunk