#include "DynamicAction.hpp"
#include <grunk/Recipe.hpp>
#include <grunk/common/io_error.hpp>

namespace grunk {

ResultHolder<DynamicAction> DynamicAction::deserialize(
    YAML::Node const& node,
    Recipe const& recipe
)
{
    auto const& features = recipe.get_features();
    std::string function_name = node.Tag();

    std::vector<DynamicFeature> input_vec;
    auto const inputs = node[1];
    for (auto const& input: inputs) {
        if (input.Tag() == "" || input.Tag() == "?") {
            // input is a named feature
            auto input_name = input.as<std::string>();
            auto feature_it = features.find(input_name);
            if (feature_it == std::end(features)) {
                throw io_error(
                    "Could not find input "s
                        + input_name + " for function call to " + function_name
                        + ". Are the steps in the correct topological order?"
                );
            }
            input_vec.push_back(feature_it->second);
        } else {
            // input is a constant
            input_vec.push_back(
                grunk::Feature(
                    "",
                    grunk::details::deserialize(input.Tag(), input)
                )
            );
        }
    }

    auto const& v = input_vec; // make sure correct overload of grunk::action is chosen.
    return grunk::action("", function_name, v);

}

ResultHolder<DynamicAction> action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args)
{
    return details::DynamicActionFactory::new_action(id, fun, args);
}

ResultHolder<DynamicAction> action(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args)
{
    std::vector<reflect::DynamicFunction::SpecifiedArgument> specified_args;
    std::transform(
        std::begin(args),
        std::end(args),
        std::back_inserter(specified_args),
        [](DynamicFeature const& f) {
            assert(f.get_type_descriptor() != nullptr);
            return reflect::DynamicFunction::SpecifiedArgument{
                f.get_type_descriptor(),
                reflect::DynamicFunction::ArgumentSpecifier::PtrOrRefToConst
            };
        }
    );
    auto const& overload = reflect::resolve_function(name);
    auto const& function = overload.resolve(specified_args);
    return action(id, function, args);
}

} //namespace grunk
