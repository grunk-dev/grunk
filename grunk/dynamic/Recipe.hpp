#include <grunk/dynamic/DynamicFeature.hpp>
#include <initializer_list>

namespace grunk {

class Recipe 
{
    using FeatureContainer = std::unordered_map<std::string, DynamicFeature>;
    using RecipeContainer = std::unordered_map<std::string, std::unique_ptr<Recipe>>;

public:
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

    DynamicFeature& at(std::string const&);
    DynamicFeature const& at(std::string const&) const;
    void insert_feature(DynamicFeature const&);
    size_t num_features() const;

    std::unique_ptr<Recipe>& get_recipe(std::string const&);
    std::unique_ptr<Recipe> const& get_recipe(std::string const&) const;
    void insert_recipe(std::string const& id, std::unique_ptr<Recipe>&&);
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
        static Action deserialize(
            YAML::Node const&,
            FeatureContainer const&
        );

    private:
        Action(std::string const& name, Recipe const&, std::initializer_list<IDPair> const& output_ids);

        std::string name;
        std::vector<std::string> input_ids;
        std::vector<IDPair> output_ids;
        std::shared_ptr<Recipe> recipe;
    };

    FeatureContainer operator()(
        std::string const& name,
        std::initializer_list<Recipe::IDPair> const& output_ids,
        FeatureContainer const& inputs
    ) const;

private:

    FeatureContainer features;
    RecipeContainer recipes;
};

template <typename... Args>
YAML::Node serialize(Args&&... args)
{
    Recipe r(std::forward<Args>(args)...);
    return r.serialize();
}

template <typename... Args>
std::string to_string(Args&&... args)
{
    YAML::Emitter out;
    out << serialize(std::forward<Args>(args)...);
    return out.c_str();
}

} // namespace grunk