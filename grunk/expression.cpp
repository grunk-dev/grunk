#include <grunk/expression.hpp>

namespace grunk {

DynamicFeature expression(std::string const& id, std::string const& expr, std::vector<DynamicFeature> const& args)
{
    return parametric::compute(std::shared_ptr<Expression>(new Expression(id, expr)), args);
}

} // namespace grunk