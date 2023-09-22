#include <grunk/dynamic/DynamicFeature.hpp>
#include <initializer_list>

namespace grunk {

class Recipe 
{
    using FeatureContainer = std::unordered_map<std::string, DynamicFeature>;
    using RecipeContainer = std::unordered_map<std::string, std::unique_ptr<Recipe>>;

public:
    Recipe(std::initializer_list<DynamicFeature> const&);
    Recipe(FeatureContainer const&);

    Recipe clone() const;

    DynamicFeature& at(std::string const&);
    DynamicFeature const& at(std::string const&) const;
    size_t size() const;

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

    private:
        Action(Recipe const&, std::unordered_map<std::string, std::string> const&);

        std::vector<std::string> input_ids;
        std::vector<std::string> output_ids;
        std::unordered_map<std::string, std::string> output_id_map;
        std::shared_ptr<Recipe> recipe;
    };

    FeatureContainer operator()(
        std::unordered_map<std::string, std::string> ouput_ids,
        FeatureContainer const& inputs
    ) const;

private:

    FeatureContainer features;
    RecipeContainer recipes;
};

} // namespace grunk