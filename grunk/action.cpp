#include <grunk/action.hpp>

namespace grunk {

ResultHolder<DynamicAction> action(std::string const& id, reflect::DynamicFunction const& fun, std::vector<DynamicFeature> const& args)
{
    return details::DynamicActionFactory::new_action(id, fun, args);
}

ResultHolder<DynamicAction> action(std::string const& id, std::string const& name, std::vector<DynamicFeature> const& args)
{
    std::vector<reflect::Parameter> params;
    std::transform(
        std::begin(args),
        std::end(args),
        std::back_inserter(params),
        [](DynamicFeature const& f) {
            assert(f.get_type_descriptor() != nullptr);
            return reflect::Parameter{
                f.get_type_descriptor(),
                reflect::Parameter::Specifier::PtrOrRefToConst
            };
        }
    );
    auto const& overload = reflect::resolve_function(name);
    auto const& function = overload.resolve(params);
    return action(id, function, args);
}


} // namespace grunk
