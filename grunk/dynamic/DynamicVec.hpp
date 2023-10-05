#pragma once 

#include <grunk/core/Feature.hpp>
#include <grunk/parametric_core.hpp>
#include <grunk/helper/common.hpp>
#include <grunk/core/Vec.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/dynamic/Recipe.hpp>

#include <vector>

namespace grunk {

DynamicFeature vec(std::string const& id, std::vector<DynamicFeature> const& args);

template <>
class Vec<reflect::DynamicObject> : public parametric::ComputeNode<Vec<reflect::DynamicObject>, parametric::Results<reflect::DynamicObject>, details::ignore>
{

    friend DynamicFeature vec(std::string const& id, std::vector<DynamicFeature> const& args);
    Vec() = default;

public:

    void connect_inputs(std::vector<DynamicFeature> const& inputs) 
    {
        for (auto const& input : inputs) {
            this->depends_on(input.param());
        }
    }

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

    std::string serialize() const override final
    {
        YAML::Node s;

        YAML::Node o;
        for (auto const& c : this->childs) {
            if ( !c.expired() ) {
                o.push_back(c.lock()->id());
            }
        }
        s.push_back(o);

        YAML::Node i;
        for (auto const& p : this->parents) {
            i.push_back(p->id());
        }
        s.push_back(i);

        s.SetStyle(YAML::EmitterStyle::Flow);

        auto tag = YAML::VerbatimTag("vec");
        YAML::Emitter out;
        out << tag << s;
        return out.c_str();

    }
    
    static DynamicFeature deserialize(YAML::Node const& yml, Recipe::FeatureContainer const& features)
    {
        auto out_id = yml[0][0].as<std::string>();

        std::vector<DynamicFeature> inputs;
        inputs.reserve(yml[1].size());
        for (auto const& input_node : yml[1]) {
            auto in_id = input_node.as<std::string>();
            inputs.push_back(features.at(in_id));
        }

        return vec(out_id, inputs);
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
    return vec(id, std::vector{f, fs...});
}

} // namespace grunk