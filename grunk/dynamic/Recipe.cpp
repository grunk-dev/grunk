#include "Recipe.hpp"
#include <unordered_map>

namespace grunk {

Recipe::Recipe(std::initializer_list<DynamicFeature> const& feature_vec)
 : recipes{}
{
    for (auto const& f : feature_vec) {
        features.emplace(f.id(), f);
    }
}

Recipe::Recipe(Recipe::FeatureContainer const& other) : features(other), recipes{} {}

DynamicFeature& Recipe::at(std::string const& id) 
{
    return features.at(id);
}

DynamicFeature const& Recipe::at(std::string const& id) const
{
    return features.at(id);
}

size_t Recipe::size() const {
    return features.size();
}

Recipe Recipe::clone() const 
{
    auto cloned_nodes = parametric::DAGNode::new_cloned_node_map();
    FeatureContainer cloned;
    for (auto const& [id, f] : features) {
        cloned.emplace(id, DynamicFeature(f.param().clone(cloned_nodes)));
    }
    return Recipe(cloned);
}

Recipe::Action::Action(Recipe const& other, std::unordered_map<std::string, std::string> const& oim)
 : output_id_map(oim)
 , recipe(std::make_shared<Recipe>(std::move(other.clone())))
 {}

void Recipe::Action::connect_inputs(Recipe::FeatureContainer const& inputs) 
{
    input_ids.clear();
    for (auto const& [id, feature] : inputs) {
        depends_on(feature.param());
        input_ids.push_back(id);
    }
}

Recipe::FeatureContainer Recipe::Action::initialize_results()
{
    output_ids.clear();
    FeatureContainer features;
    for (auto const& [id_to, id_from] : output_id_map) {
        features.emplace(
            id_to,
            DynamicFeature(
                parametric::new_param<reflect::DynamicObject>(), 
                recipe->at(id_from).get_type_descriptor()
            )
        );
        output_ids.push_back(id_from);
    }
    return features;
}

void Recipe::Action::connect_results(Recipe::FeatureContainer const& res)
{
    for (auto const& [idout, feature] : res) {
        computes(feature.param());
    }
}

void Recipe::Action::post_connect()
{
    for (size_t i=0; i < this->num_children(); ++i) {
        if (auto r = this->template res<reflect::DynamicObject>(i); r) {
            r->set_id("TODO");
        }
    }
}

void Recipe::Action::eval() const
{
    for (size_t i = 0; i < this->num_parents(); ++i) {
        recipe->at(input_ids[i]).access_value() 
            = this->template arg<reflect::DynamicObject>(i).value();
    }

    for (size_t i=0; i < this->num_children(); ++i) {
        if (auto r = this->template res<reflect::DynamicObject>(i); r) {
            r->set_value(recipe->at(output_ids[i]).value());
        }
    }
}

FeatureContainer Recipe::operator()(
    std::unordered_map<std::string, std::string> output_ids,
    FeatureContainer const& inputs
) const
{
    auto ptr = std::shared_ptr<Recipe::Action>(
        new Recipe::Action(
            this->clone(),
            output_ids
        )
    );

    return parametric::compute(ptr, inputs);
}

} // namespace grunk
