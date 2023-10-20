#include <grunk/dynamic/Recipe.hpp>
#include <grunk/dynamic/Expression.hpp>
#include <grunk/dynamic/Script.hpp>
#include <grunk/dynamic/DynamicVec.hpp>
#include <grunk/io/io.hpp>
#include <stdexcept>
#include <string>

namespace grunk {

Recipe::Recipe(std::initializer_list<DynamicFeature> const& feature_vec)
 : recipes{}
{
    for (auto const& f : feature_vec) {
        auto ret = features.emplace(f.id(), f);
        if (!ret.second) {
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

    //write grunk version
    root["uses"]["grunk"] = grunk_VERSION;
    
    grunk::details::ToStringVisitor visitor(root);
    for (auto const& kv: features){
        grunk::details::parse_feature(kv.second, visitor);
    }
    visitor.unwind_steps();

    for (auto const& kv : visitor.feature_names_count) {
        if (kv.second > 1) {
            using namespace std::string_literals;
            throw io_error("The feature tree does not have unique feature names: \""s + kv.first +"\" appears " + std::to_string(kv.second) + " times");
        }
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

            auto ptr = std::make_shared<Recipe>(
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
                Recipe::Action::deserialize(
                    steps[i], 
                    recipe    
                ); 
            } else if (function_name == "script") {
                auto outputs = Script::deserialize(steps[i], recipe.features);
                for (size_t i = 0; i < outputs.size(); ++i) {
                    auto const& output = outputs.output(i);
                    auto output_name = output.id();
                    if (recipe.features.find(output_name) != recipe.features.end()) {
                        throw io_error("Error parsing step " + std::to_string(i) + ": A parameter with name \"" + output_name + "\" already exists.");
                    }
                    recipe.insert_feature(output);
                }
            } else if (function_name == "vec") {
                auto output = DynamicVec::deserialize(steps[i], recipe.features);
                auto output_name = output.id();
                if (recipe.features.find(output_name) != recipe.features.end()) {
                    throw io_error("Error parsing step " + std::to_string(i) + ": A parameter with name \"" + output_name + "\" already exists.");
                }
                recipe.features.emplace(output_name, output);
            } else {

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

Recipe::FeatureContainer const& Recipe::get_features() const
{
    return features;
}

Recipe::FeatureContainer& Recipe::get_features()
{
    return features;
}

DynamicFeature& Recipe::at(std::string const& id) 
{
    // return feature if it is in feature contaiiner
    if (features.find(id) != features.end()) {
        return features.at(id);
    }

    //TODO: It would be cool if we could walk the tree up to look for ancestors with the id
    using namespace std::string_literals;
    throw std::out_of_range("Cannot find feature with id \"" + id + "\" in recipe.");
}

DynamicFeature const& Recipe::at(std::string const& id) const
{
    // return feature if it is in feature contaiiner
    if (features.find(id) != features.end()) {
        return features.at(id);
    }

    //TODO: It would be cool if we could walk the tree up to look for ancestors with the id
    using namespace std::string_literals;
    throw std::out_of_range("Cannot find feature with id \"" + id + "\" in recipe.");
}

DynamicFeature& Recipe::operator[](std::string const& id)
{
    return features.at(id);
}

DynamicFeature const& Recipe::operator[](std::string const& id) const
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

Recipe& Recipe::get_recipe(std::string const& id)
{
    using namespace std::string_literals;
    if (auto& ptr = recipes.at(id); ptr) {
        return *recipes.at(id);
    } else {
        throw std::logic_error("Recipe \""s + id + "\" is null.");
    }
}

Recipe const& Recipe::get_recipe(std::string const& id) const
{
    using namespace std::string_literals;
    if (auto& ptr = recipes.at(id); ptr) {
        return *recipes.at(id);
    } else {
        throw std::logic_error("Recipe \""s + id + "\" is null.");
    }
}

void Recipe::insert_recipe(std::string const& id, std::shared_ptr<Recipe>&& r)
{
    recipes.insert({id, std::move(r)});
}

void Recipe::insert_recipe(std::string const& id, Recipe&& recipe)
{
    recipes.insert({id, std::make_shared<Recipe>(std::move(recipe))});
}

size_t Recipe::num_recipes() const {
    return recipes.size();
}

Recipe Recipe::clone() const 
{
    auto cloned_nodes = parametric::DAGNode::new_cloned_node_map();
    FeatureContainer cloned;
    for (auto const& [id, f] : features) {
        cloned.emplace(
            id, 
            DynamicFeature(
                f.param().clone(cloned_nodes), 
                f.get_type_descriptor()
            )
        );
    }

    Recipe out(cloned);

    for (auto const& [id, r] : recipes) {
        out.insert_recipe(id, std::move(r->clone()));
    }
    return out;
}

Recipe::Action::Action(
    std::string const& n, 
    Recipe const& other, 
    std::vector<Recipe::IDPair> const& oid)
 : m_name(n)
 , output_ids(oid)
 , m_recipe(std::make_shared<Recipe>(std::move(other.clone())))
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
                m_recipe->at(id_pair.id_from).get_type_descriptor()
            )
        );
    }
    return features;
}

void Recipe::Action::connect_results(Recipe::FeatureContainer const& res)
{
    int i =0;
    for (auto const& id_pair : output_ids) {
        computes(res.at(id_pair.id_to).param());
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
        m_recipe->at(input_ids[i]).access_value() 
            = this->template arg<reflect::DynamicObject>(i).value();
    }

    for (size_t i=0; i < this->num_children(); ++i) {
        if (auto r = this->template res<reflect::DynamicObject>(i); r) {
            r->set_value(m_recipe->at(output_ids[i].id_from).value());
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
        auto const& input = this->template arg<reflect::DynamicObject>(i);
        if (input.id() == "" && input.num_parents() == 0) {
            // this is a constant
            inputs[input_ids[i]] = YAML::Load(input.serialize());
        } else {
            inputs[input_ids[i]] = input.id();
        }
    }
    s.push_back(inputs);

    s.SetStyle(YAML::EmitterStyle::Flow);
    YAML::Emitter out;

    using namespace std::string_literals;
    auto tag = YAML::VerbatimTag("recipes::"s + name());
    out << tag << s;
    return out.c_str();
}

void Recipe::Action::deserialize(
    YAML::Node const& node,
    Recipe& recipe
)
{
    assert(node.size() == 2);

    // get subrecipe name from tag
    std::string tag = node.Tag();
    std::string recipe_name = tag.substr(9); // everything after recipes::
    
    // construct output map
    std::vector<Recipe::IDPair> output_ids;
    for (YAML::const_iterator it=node[0].begin();it!=node[0].end();++it ) {
        auto key = it->first.as<std::string>();
        auto val = it->second.as<std::string>();
        output_ids.push_back(IDPair{key, val});
    }

    // construct input map
    FeatureContainer inputs;
    for (YAML::const_iterator it=node[1].begin();it!=node[1].end();++it ) {
        auto key = it->first.as<std::string>();
        if (it->second.Tag() == "" || it->second.Tag() == "?") {
            // input is a named feature
            auto val = it->second.as<std::string>();
            inputs.insert({key, recipe.features.at(val)});
        } else {
            inputs.insert(
                {
                    key,
                    grunk::Feature(
                        "",
                        grunk::details::deserialize(it->second.Tag(), it->second)
                    )
                }
            );
        }
        
    }

    // call the subrecipe 
    recipe.recipe(
        recipe_name,
        output_ids, 
        inputs
    );
}

std::string Recipe::Action::name() const 
{
    return m_name;
}

Recipe::FeatureContainer Recipe::operator()(
    std::string const& name,
    std::vector<Recipe::IDPair> const& output_ids,
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

void Recipe::recipe(
    std::string const& name,
    std::vector<Recipe::IDPair> const& output_ids,
    FeatureContainer const& inputs
)
{
    auto outputs = get_recipe(name)(name, output_ids, inputs);
    for (auto const& kv : outputs) {
        insert_feature(kv.second);
    }
}

} // namespace grunk
