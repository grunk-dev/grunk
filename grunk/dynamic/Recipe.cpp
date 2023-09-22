#include <grunk/dynamic/Recipe.hpp>
#include <grunk/dynamic/Expression.hpp>
#include <grunk/io/io.hpp>

namespace grunk {

Recipe::Recipe(std::initializer_list<DynamicFeature> const& feature_vec)
 : recipes{}
{
    for (auto const& f : feature_vec) {
        auto ret = features.emplace(f.id(), f);
        if (not ret.second) {
            // no insertion took place
            if (features.find(f.id()) != features.end()) {
                throw std::logic_error("A feature with id \""s + f.id() + "\" already exists. Duplicate fature ids are not allowed");
            }
        }
    }
}

Recipe::Recipe(Recipe::FeatureContainer const& other) : features(other), recipes{} {}


YAML::Node Recipe::serialize() const
{
    YAML::Node root;
    grunk::details::Visited visited;    

    //write grunk version
    root["uses"]["grunk"] = grunk_VERSION;
    
    for (auto const& value: features){
        grunk::details::parse_feature(value.second, root, visited);
    }

    if (!grunk::details::has_unique_feature_names(root)) {
        throw io_error("The feature tree does not have unique feature names.");
    }

    // write loaded plugins
    auto const& registry = get_plugin_registry();
    for(auto const& [name, entry] : registry.plugins()){
        root["uses"][name] = entry.plugin->version();
    }

    // write recipes
    if (recipes.size() > 0) {
        root["recipes"] = YAML::Node();
        for (auto const& [key, value] : recipes) {
            root["recipes"][key] = value->serialize();
        }
    }

    return root;
}

Recipe Recipe::deserialize(YAML::Node const& root)
{
    if (!root["uses"]) {
        throw io_error("Missing \"uses\" block.");
    }

    auto const uses = root["uses"];
    if (!uses["grunk"])
    {
        throw io_error("Missing \"grunk\" in \"uses\" block.");
    }

    try {
        auto ver = uses["grunk"].as<std::string>();
        if (ver != grunk_VERSION) {
            //TODO: Generate a meaningful warning. Throwing an exception is not a 
            // viable solution. This will be done here anyway as long as grunk is in experimental state.
            throw io_error("Parsed version "s + ver + " does not match grunk version " + grunk_VERSION);
        }
    }
    catch (std::exception const& e) 
    {
        throw io_error(e.what());
    }

    //TODO: Parse plugins from input file and compare with loaded plugins. Handle appropriately


    Recipe recipe;

    if (auto const& recipes_node = root["recipes"]; recipes_node) {
        for (YAML::const_iterator it=recipes_node.begin();it!=recipes_node.end();++it ) {
            auto name = it->first.as<std::string>();

            auto ptr = std::make_unique<Recipe>(
                std::move(Recipe::deserialize(it->second))
            );
            recipe.recipes.emplace(name, std::move(ptr));
        }
    }

    if (auto const parameters = root["parameters"]; parameters) {
        for (YAML::const_iterator it=parameters.begin();it!=parameters.end();++it ) {
            
            auto name = it->first.as<std::string>();
            auto type = it->second.Tag();
            auto value = it->second;

            if (recipe.features.find(name) != recipe.features.end()) {
                throw io_error("Error parsing parameters. A parameter with name \"" + name + "\" already exists.");
            }

            auto object = grunk::details::deserialize(type, value);
            recipe.features.emplace(name, DynamicFeature(name, std::move(object)));
        }
    }

    if (auto const steps = root["steps"]; steps)
    {
        for (size_t i = 0; i < steps.size(); i++) {
            
            auto const function_name = steps[i].Tag();

            if (function_name == "expr") {

                auto output = Expression::deserialize(steps[i], recipe.features);
                auto output_name = steps[i][0].as<std::string>();
                if (recipe.features.find(output_name) != recipe.features.end()) {
                    throw io_error("Error parsing step " + std::to_string(i) + ": A parameter with name \"" + output_name + "\" already exists.");
                }
                output.set_id(output_name);
                recipe.features.emplace(output_name, output);

            } else if (auto n = function_name.rfind("recipes::", 0); n == 0) {
                std::string recipe_name = function_name.substr(n+1);
                auto outputs = Recipe::Action::deserialize(
                    node, 
                    recipe    
                ); 

                // TODO: / insert output features into recipe
            } else


                auto output_nodes = DynamicAction::deserialize(steps[i], recipe.features);

                auto const outputs = steps[i][0];
                if (outputs.size() != output_nodes.size()) {
                    throw io_error("Number of given outputs doesn't match number of outputs of function "s + function_name);
                }

                size_t idx = 0;
                for (auto const& node : outputs) {
                    auto output_name = node.as<std::string>();

                    if (recipe.features.find(output_name) != recipe.features.end()) {
                        throw io_error("Error parsing step " + std::to_string(i) + ": A parameter with name \"" + output_name + "\" already exists.");
                    }

                    auto output = output_nodes.output(idx++);
                    output.set_id(output_name);
                    recipe.features.emplace(output_name, output);
                }
            }
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

Recipe::Action::Action(
    std::string const& n, 
    Recipe const& other, 
    std::initializer_list<Recipe::IDPair> const& oid)
 : name(n)
 , output_ids(oid)
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

std::string Recipe::Action::serialize() const
{
    YAML::Node s;

    YAML::Node outputs;
    for (auto const& id_pair : output_ids) {
        outputs[id_pair.id_to] = id_pair.id_from;
    }
    s.push_back(outputs);

    YAML::Node inputs;
    for (size_t i=0; i < this->num_parents(); ++i) {
        auto id_from = this->template arg<reflect::DynamicObject>(i).id();
        inputs[input_ids[i]] = id_from;
    }
    s.push_back(inputs);

    s.SetStyle(YAML::EmitterStyle::Flow);
    YAML::Emitter out;

    using namespace std::string_literals;
    auto tag = YAML::VerbatimTag("recipes::"s + name);
    out << tag << s;
    return out.c_str();
}

Recipe::Action Recipe::Action::deserialize(
    YAML::Node const& node,
    Recipe const& recipe
)
{
    assert(node.size() == 2);

    // get subrecipe name from tag
    std::string tag = node.Tag();
    std::string recipe_name = tag.substr(9); // everything after recipes::

    // get subrecipe from input recipe
    auto subrecipe = recipe.get_recipe(recipe_name);
    
    // construct output map
    // construct input map
    // call the subrecipe 
    /
}

FeatureContainer Recipe::operator()(
    std::string const& name,
    std::initializer_list<Recipe::IDPair> const& output_ids,
    FeatureContainer const& inputs
) const
{
    auto ptr = std::shared_ptr<Recipe::Action>(
        new Recipe::Action(
            name,
            this->clone(),
            output_ids
        )
    );

    return parametric::compute(ptr, inputs);
}

} // namespace grunk
