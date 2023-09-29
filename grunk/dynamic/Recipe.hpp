#include <grunk/dynamic/DynamicFeature.hpp>
#include <grunk/io/io.hpp>

#include <initializer_list>

namespace grunk {

class Recipe 
{
public:
    using FeatureContainer = std::unordered_map<std::string, DynamicFeature>;
    using RecipeContainer = std::unordered_map<std::string, std::shared_ptr<Recipe>>;

    Recipe() = default;
    Recipe(std::initializer_list<DynamicFeature> const&);
    Recipe(FeatureContainer const&);

    template <typename... Args>
    Recipe(Feature<Args> const&... args)
     : Recipe({args...}) 
    {}

    YAML::Node serialize() const;
    static Recipe deserialize(YAML::Node const&);

    Recipe clone() const;

    FeatureContainer const& get_features() const;
    FeatureContainer& get_features();
    DynamicFeature& at(std::string const&);
    DynamicFeature const& at(std::string const&) const;
    DynamicFeature& operator[](std::string const&);
    DynamicFeature const& operator[](std::string const&) const;
    void insert_feature(DynamicFeature const&);
    size_t num_features() const;

    template <typename... Args>
    void feature(std::string const& id, Args&&... args) {
        features.emplace(id, DynamicFeature(id, std::forward<Args>(args)...));
    }

    Recipe& get_recipe(std::string const&);
    Recipe const& get_recipe(std::string const&) const;
    void insert_recipe(std::string const& id, std::shared_ptr<Recipe>&&);
    void insert_recipe(std::string const& id, Recipe&&);
    size_t num_recipes() const;

    struct IDPair 
    {
        std::string id_to;
        std::string id_from;
    };

    struct ignore {};
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

    FeatureContainer operator()(
        std::string const& name,
        std::vector<Recipe::IDPair> const& output_ids,
        FeatureContainer const& inputs
    ) const;

    void recipe(
        std::string const& name,
        std::vector<Recipe::IDPair> const& output_ids,
        FeatureContainer const& inputs
    );

private:

    FeatureContainer features;
    RecipeContainer recipes;
};

template <typename... Args>
YAML::Node serialize(Feature<Args> const&... args)
{
    Recipe r(args...);
    return r.serialize();
}

} // namespace grunk