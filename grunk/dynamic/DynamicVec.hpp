#pragma once 

#include <grunk/core/Feature.hpp>
#include <grunk/parametric_core.hpp>
#include <grunk/core/Vec.hpp>
#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/dynamic/Recipe.hpp>

#include <vector>

namespace grunk {

DynamicFeature vec(std::string const& id, std::vector<DynamicFeature> const& args);

/**
 * @brief This class represents a compute node, that maps several DynamicFeatures, where each is assumed to 
 * type-erase the same type T, to a DynamicFeature type-erasing an std::vector<reflect::DynamicObject> containing
 * the input values. 
 *
 * This class must be constructed via the factory function ::grunk::vec.
 * 
 * @tparam  
 */
template <>
class Vec<reflect::DynamicObject> : public parametric::ComputeNode<Vec<reflect::DynamicObject>, parametric::Results<reflect::DynamicObject>>
{

    friend DynamicFeature vec(std::string const& id, std::vector<DynamicFeature> const& args);
    Vec() = default;

public:

    /**
     * @brief connect this compute node to the inputs
     * 
     * @param inputs
     */
    void connect_inputs(std::vector<DynamicFeature> const& inputs) 
    {
        for (auto const& input : inputs) {
            this->depends_on(input.param());
        }
    }

    /**
     * @brief evaluate this compute node
     * 
     */
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

    /**
     * @brief serializes this compute node to yaml
     * 
     * @return std::string 
     */
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
            if ( p->id() == "" && p->num_parents() == 0) {
                // p is a constant
                YAML::Node n = YAML::Load(p->serialize());
                i.push_back(n);
            } else {
                i.push_back(p->id());
            }
        }
        s.push_back(i);

        s.SetStyle(YAML::EmitterStyle::Flow);

        auto tag = YAML::VerbatimTag("vec");
        YAML::Emitter out;
        out << tag << s;
        return out.c_str();

    }
    
    /**
     * @brief desersializes this compute node from yaml. 
     * 
     * @param yml 
     * @param features 
     * @return DynamicFeature 
     */
    static DynamicFeature deserialize(YAML::Node const& yml, Recipe::FeatureContainer const& features)
    {
        auto out_id = yml[0][0].as<std::string>();

        std::vector<DynamicFeature> inputs;
        inputs.reserve(yml[1].size());
        for (auto const& input_node : yml[1]) {
            if (input_node.Tag() == "" || input_node.Tag() == "?") {
                // input_node is a named feature
                auto in_id = input_node.as<std::string>();
                inputs.push_back(features.at(in_id));
            }
            else {
                inputs.push_back(
                    grunk::Feature(
                        "",
                        grunk::details::deserialize(input_node.Tag(), input_node)
                    )
                );
            }
        }

        return vec(out_id, inputs);
    }
};

using DynamicVec = Vec<reflect::DynamicObject>;

namespace {
    // needed because of bug in MSVC 2017: Can't use a fold expression in std::enable_if_t of vec definition
    template <typename... Ts>
    constexpr bool is_dynamic_feature_v = std::is_same_v<std::tuple<DynamicFeature, Ts...>, std::tuple<Ts..., DynamicFeature>>;
}

/**
 * @ingroup dynamic
 * @brief creates a DynamicFeature type-erasing an std::vector<reflect::DynamicObject> from several DynamicFeatures.
 * 
 * @tparam Args The DynamicFeatures
 * @param id id of the returned feature
 * @param f The first input feature
 * @param fs The remaining input Features
 * @return DynamicFeature
 */
template <typename... Args>
std::enable_if_t<is_dynamic_feature_v<Args...>, DynamicFeature> 
vec(std::string const& id, DynamicFeature const& f, Args const&... fs)
{
    return vec(id, std::vector{f, fs...});
}

/**
 * @ingroup advanced
 * @brief throws an exception. ::grunk::vec should be called with at least one argument.
 * 
 * @return DynamicFeature 
 */
inline DynamicFeature vec(std::string) {
    throw std::logic_error("grunk::vec must have at least one argument.");
}

} // namespace grunk