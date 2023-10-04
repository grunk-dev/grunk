#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/io/io.hpp>

#include <initializer_list>

namespace grunk {

/**
 * @ingroup dynamic
 * @brief The Recipe class represents a parametric feature tree. It stores several
 * DynamicFeatures in a map. These dynamic features represent the tree. In addition, 
 * it stores a map of subrecipes, which can be used like functions within the outer recipe.
 * 
 */
class Recipe 
{
public:
    using FeatureContainer = std::unordered_map<std::string, DynamicFeature>;
    using RecipeContainer = std::unordered_map<std::string, std::shared_ptr<Recipe>>;

    Recipe() = default;

    /**
     * @brief Construct a new Recipe object from a list of DynamicFeature instances
     * 
     */
    Recipe(std::initializer_list<DynamicFeature> const&);

    /**
     * @brief Construct a new Recipe object from a a map of DynamicFeatures
     * 
     */
    Recipe(FeatureContainer const&);

    /**
     * @brief Construct a new Recipe object from a parameter pack of DynamicFeature instances
     * 
     * @tparam Args 
     * @param args 
     */
    template <typename... Args>
    Recipe(Feature<Args> const&... args)
     : Recipe({args...}) 
    {}

    /**
     * @brief serializes a recipe to yaml. This is used to write grunk recipes to file
     * 
     * @return YAML::Node A YAML::Node instance from the yaml-cpp library storing a yaml representation of the recipe
     */
    YAML::Node serialize() const;

    /**
     * @brief deserializes a recipe from yaml. This is used to read grunk recipes from file
     * 
     * @return Recipe a new Recipe instance that is reconstructed from its yaml representation
     */
    static Recipe deserialize(YAML::Node const&);

    /**
     * @brief deep-copies a recipe. This creates a full copy of the feature tree with the same parent-child relations
     * 
     * @return Recipe the deep-copied recipe
     */
    Recipe clone() const;

    /**
     * @brief Returns the map of features
     * 
     * @return FeatureContainer const& the map of features
     */
    FeatureContainer const& get_features() const;

    /**
     * @brief Returns the map of features
     * 
     * @return FeatureContainer& the map of features
     */
    FeatureContainer& get_features();

    /**
     * @brief retrieval of a DynamicFeature in the recipe
     * 
     * @return DynamicFeature& 
     */
    DynamicFeature& at(std::string const&);

    /**
     * @brief retrieval of a DynamicFeature in the recipe
     * 
     * @return DynamicFeature const& 
     */
    DynamicFeature const& at(std::string const&) const;

    /**
     * @brief retrieval of a DynamicFeature in the recipe
     * 
     * @return DynamicFeature& 
     */
    DynamicFeature& operator[](std::string const&);

    /**
     * @brief retrieval of a DynamicFeature in the recipe
     * 
     * @return DynamicFeature const& 
     */
    DynamicFeature const& operator[](std::string const&) const;

    /**
     * @brief inserts a feature into the recipe
     * 
     */
    void insert_feature(DynamicFeature const&);

    /**
     * @brief returns the number of features explicitly stored in the recipe.
     * Note that some features may be implicitly stored in the parent-child relations
     * of the features, but not explicitly as an entry in the map of features.
     * 
     * @return size_t 
     */
    size_t num_features() const;

    /**
     * @brief creats a new feature in the recipe
     * 
     * @tparam Args additional argument types for ::grunk::DynamicFeature
     * @param id id of the new feature
     * @param args additional arguments passed to the ::grunk::DynamicFeature constructor
     */
    template <typename... Args>
    void feature(std::string const& id, Args&&... args) {
        features.emplace(id, DynamicFeature(id, std::forward<Args>(args)...));
    }

    /**
     * @brief retrieval of a subrecipe by id
     * 
     * @return Recipe&
     */
    Recipe& get_recipe(std::string const&);

    /**
     * @brief retrieval of a subrecipe by id
     * 
     * @return Recipe const& 
     */
    Recipe const& get_recipe(std::string const&) const;

    /**
     * @brief inserts a new subrecipe and assigns it an id/key.
     * 
     * @param id 
     */
    void insert_recipe(std::string const& id, std::shared_ptr<Recipe>&&);

    /**
     * @brief inserts a new subrecipe and assigns it an id/key.
     * 
     * @param id 
     */
    void insert_recipe(std::string const& id, Recipe&&);

    /**
     * @brief returns the number of subrecipes
     * 
     * @return size_t 
     */
    size_t num_recipes() const;

    /**
     * @brief a helper struct to pass arguments to ::grunk::Recipe::operator(), i.e. 
     * to use a feature tree / recipe like a compute node / action.
     * 
     */
    struct IDPair 
    {
        std::string id_to;
        std::string id_from;
    };

    // helper struct to explicitly mark the template parameter of ComputeNode as ignored
    struct ignore {};

    /**
     * @private
     * @brief This class represents the use of a ::grunk::Recipe as a ::grunk::Action
     * in a feature tree.
     * 
     */
    class Action : public parametric::ComputeNode<Action, ignore, ignore>
    {
        friend class Recipe;

    public:

        void connect_inputs(FeatureContainer const& inputs);
        FeatureContainer initialize_results();
        void connect_results(FeatureContainer const&);
        void post_connect();

        void eval() const override;

        std::string serialize() const override final;
        static void deserialize(
            YAML::Node const&,
            Recipe& recipe
        );

    private:
        Action(std::string const& name, Recipe const&, std::vector<IDPair> const& output_ids);

        std::string name;
        std::vector<std::string> input_ids;
        std::vector<IDPair> output_ids;
        std::shared_ptr<Recipe> recipe;
    };

    /**
     * @brief creates a new compute node representing the evaluation of the recipe like a function
     * 
     * @param name assign a name to the newly created compute node
     * @param output_ids a list of ::grunk::Recipe::IDPair instances. The id_from value corresponds to an id of 
     *                   an existing feature in the recipe, while the id_to value corresponds to a new output feature
     *                   that shall be generated as an output of the compute node.
     * @param inputs a map of input features. The keys are the ids of existing features within the recipe
     *               and the values are the input features of the compute node
     * @return FeatureContainer a map of newly created output features where the keys are the ids as specified
     *                          in the output_ids argument.
     */
    FeatureContainer operator()(
        std::string const& name,
        std::vector<Recipe::IDPair> const& output_ids,
        FeatureContainer const& inputs
    ) const;

    /**
     * @brief creates a new compute node representing the evaluation of a subrecipe with a a given name.
     * The output features will be stored directly within the outer recipe
     * 
     * @param name The name of the subrecipe
     * @param output_ids a list of ::grunk::Recipe::IDPair instances. See ::grunk::Recipe::operator() for more 
     *                   information.
     * @param inputs a map of input features. See ::grunk::Recipe::operator() for more information.
     */
    void recipe(
        std::string const& name,
        std::vector<Recipe::IDPair> const& output_ids,
        FeatureContainer const& inputs
    );

private:

    FeatureContainer features;
    RecipeContainer recipes;
};

/**
 * @brief serialize Features to yaml
 * 
 * @tparam Args the types stored in the features
 * @param args the featues to be serialized
 * @return YAML::Node a yaml representation of a recipe containing the input features
 */
template <typename... Args>
YAML::Node serialize(Feature<Args> const&... args)
{
    Recipe r(args...);
    return r.serialize();
}

} // namespace grunk