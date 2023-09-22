#include <grunk/dynamic/Recipe.hpp>
#include <grunk/io/io.hpp>

namespace grunk {

Recipe::Recipe(std::initializer_list<DynamicFeature> const& feature_vec)
 : recipes{}
{
    for (auto const& f : feature_vec) {
        features.emplace(f.id(), f);
    }
}

Recipe::Recipe(Recipe::FeatureContainer const& other) : features(other), recipes{} {}

YAML::Node Recipe::serialize() const
{
    YAML::Node root = details::feature_tree_to_yaml(features);
    if (recipes.size() > 0) {
        root["recipes"] = YAML::Node();
        for (auto const& [key, value] : recipes) {
            root["recipes"][key] = value->serialize();
        }
    }
    return root;
}

Recipe Recipe::deserialize(YAML::Node const& node)
{
    auto recipe = Recipe(details::yaml_to_feature_tree(node));
    if (auto const& recipes_node = node["recipes"]; recipes_node) {
        for (YAML::const_iterator it=recipes_node.begin();it!=recipes_node.end();++it ) {
            auto name = it->first.as<std::string>();

            auto ptr = std::make_unique<Recipe>(
                std::move(Recipe::deserialize(it->second))
            );
            recipe.insert_recipe(name, std::move(ptr));
        }
    }
    return recipe;
}

DynamicFeature& Recipe::at(std::string const& id) 
{
    return features.at(id);
}

DynamicFeature const& Recipe::at(std::string const& id) const
{
    return features.at(id);
}

void Recipe::insert_feature(DynamicFeature const&f)
{
    features.insert({f.id(), f});
}

size_t Recipe::num_features() const {
    return features.size();
}

std::unique_ptr<Recipe>& Recipe::get_recipe(std::string const& id)
{
    return recipes.at(id);
}

std::unique_ptr<Recipe> const& Recipe::get_recipe(std::string const& id) const
{
    return recipes.at(id);
}

void Recipe::insert_recipe(std::string const& id, std::unique_ptr<Recipe>&& r)
{
    recipes.insert({id, std::move(r)});
}

size_t Recipe::num_recipes() const {
    return recipes.size();
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

Recipe::Action::Action(Recipe const& other, std::initializer_list<Recipe::IDPair> const& oid)
 : output_ids(oid)
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
    FeatureContainer features;
    for (auto const& id_pair : output_ids) {
        features.emplace(
            id_pair.id_to,
            DynamicFeature(
                parametric::new_param<reflect::DynamicObject>(), 
                recipe->at(id_pair.id_from).get_type_descriptor()
            )
        );
    }
    return features;
}

void Recipe::Action::connect_results(Recipe::FeatureContainer const& res)
{
    for (auto const& item : res) {
        computes(item.second.param());
    }
}

void Recipe::Action::post_connect()
{
    for (size_t i=0; i < this->num_children(); ++i) {
        if (auto r = this->template res<reflect::DynamicObject>(i); r) {
            r->set_id(output_ids[i].id_to);
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
            r->set_value(recipe->at(output_ids[i].id_from).value());
        }
    }
}

FeatureContainer Recipe::operator()(
    std::initializer_list<Recipe::IDPair> const& output_ids,
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
