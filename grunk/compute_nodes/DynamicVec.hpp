#pragma once 

#include <grunk/Feature.hpp>
#include <grunk/common/parametric_core.hpp>
#include <grunk/compute_nodes/Vec.hpp>
#include <grunk/DynamicFeature.hpp>
#include <grunk/Recipe.hpp>

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
class Vec<reflect::DynamicObject> : public parametric::ComputeNode<Vec<reflect::DynamicObject>, Results<reflect::DynamicObject>>
{

    friend DynamicFeature vec(std::string const& id, std::vector<DynamicFeature> const& args);
    Vec() = default;

    inline decltype(auto) result(int i) const {
        return this->res<reflect::DynamicObject, Serializer>(i);
    }

    inline decltype(auto) argument(int i) const {
        return this->arg<reflect::DynamicObject, Serializer>(i);
    }

public:

    /**
     * @brief connect this compute node to the inputs
     * 
     * @param inputs
     */
    void connect_inputs(std::vector<DynamicFeature> const& inputs) 
    {
        for (auto const& input : inputs) {
            this->depends_on(input.get_param());
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
        for (int i=0; i<this->parents.size(); ++i) {
            v.push_back(argument(i).value());
        }
        if (auto r =  result(0); r) {
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
            i.push_back(details::serialize(*p));
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

} // namespace grunk
