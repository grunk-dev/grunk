#pragma once 

#include <grunk/core/Feature.hpp>
#include <grunk/parametric_core.hpp>
#include <grunk/helper/common.hpp>

#include <vector>

namespace grunk {

template <typename T>
class Vec : public parametric::ComputeNode<Vec<T>, parametric::Results<std::vector<T>>, details::ignore>
{
public:
    Vec() = default;
    void eval() const override final
    {
        std::vector<T> v;
        v.reserve(this->parents.size());
        for (size_t i=0; i<this->parents.size(); ++i) {
            v.push_back(this->template arg<T>(i).value());
        }
        if (auto r =  this->template res<0>(); r) {
            r->set_value(std::move(v));
        }
    }

private:
};

namespace {
    template <typename... Ts>
    using FirstType = std::tuple_element_t<0,std::tuple<Ts...>>;
}

template <typename... Ts>
Feature<std::vector<FirstType<Ts...>>> vec(std::string const& id, Feature<Ts> const&... fs)
{
    using T = FirstType<Ts...>;
    static_assert((std::is_same_v<T, Ts> && ...));
    auto ret = parametric::compute<Vec<T>>(fs.param()...);
    ret.set_id(id);
    return ret;
}

} // namespace grunk