#include <grunk/action.hpp>

namespace grunk {

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


} // namespace grunk