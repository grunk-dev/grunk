#include "Expression.hpp"
#include <grunk/dynamic/Recipe.hpp>
#include <grunk/io/io_error.hpp>
#include <unordered_map>

namespace grunk {

Expression::Expression(
    std::string const& id,
    std::string const& expression
)
    : expr(expression)
{
    set_id(id);
}

void Expression::eval() const 
{
    mu::Parser parser;
    parser.SetExpr(expr);
    std::unordered_map<std::string, double> vars;
    for (size_t i = 0; i < this->num_parents(); ++i) {
        auto input = this->template arg<reflect::DynamicObject>(i);
        auto ret = vars.emplace(
            std::make_pair(
                input.id(),
                input.value().as<double>()
            )
        );
        parser.DefineVar(
            input.id(), 
            &(ret.first->second)
        );
    }

    double res;
    try {
        res = parser.Eval();
    } catch (mu::Parser::exception_type &e) {
        throw io_error(
            "Could not parse expression \""s
            + expr 
            + "\": "
            + e.GetMsg());
    }
    auto result = reflect::DynamicObject(std::move(res));
    if (auto out = this->template res<reflect::DynamicObject>(0); out) {
        out->set_value(result);
    }
}

void Expression::post_connect() const
{
    if (auto out = this->template res<reflect::DynamicObject>(0); out) {
       out->set_id(this->id());
   }
}

std::string Expression::serialize() const
{
    YAML::Node s;
    if (auto out = this->template res<reflect::DynamicObject>(0); out) {
        s.push_back(out->id());
        s.push_back(expr);
    }

    s.SetStyle(YAML::EmitterStyle::Flow);
    YAML::Emitter out;

    auto tag = YAML::VerbatimTag("expr");
    out << tag << s;
    return out.c_str();
}

using FeatureContainer = Recipe::FeatureContainer;

DynamicFeature Expression::deserialize(
    YAML::Node const& node,
    Recipe const& recipe
)
{
    auto const& features = recipe.get_features();
    std::string expr = node[1].as<std::string>();
    mu::Parser p;
    p.SetExpr(expr);

    std::vector<DynamicFeature> input_vec;
    for (auto const& [input_name, var] : p.GetUsedVar()) {
        auto feature_it = features.find(input_name);
        if (feature_it == std::end(features)) {
            throw io_error(
                "Could not find input "s
                    + input_name + " in expression \"" + node[0].as<std::string>()
                    + " = " + expr
                    + "\". Are the steps in the correct topological order?"
            );
        }
        input_vec.push_back(feature_it->second);
    }

    return grunk::expression("", expr, std::move(input_vec));
}

DynamicFeature expression(std::string const& id, std::string const& expr, std::vector<DynamicFeature> const& args)
{
    return parametric::compute(std::shared_ptr<Expression>(new Expression(id, expr)), args);
}

} // namespace grunk
