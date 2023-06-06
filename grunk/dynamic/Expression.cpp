#include "Expression.hpp"
#include <grunk/io/io_error.hpp>

namespace grunk {

Expression::Expression(
    std::string const& id,
    std::string const& expression,
    std::vector<DynamicFeature> const& in
)
    : inputs(in)
    , expr(expression)
{
    set_id(id);
    
    for (auto& i: inputs){
        depends_on(i.param());
    }
    computes(out, parametric::param<reflect::DynamicObject>(id));
}

void Expression::eval() const 
{
    mup::ParserX parser;
    parser.SetExpr(expr);
    for (auto const& input : inputs) {
        mup::Value  val(input.value().as<double>());
        parser.DefineVar(input.id(), mup::Variable(&val));
    }

    double result = parser.Eval().GetFloat();
    if (!out.expired()) {
        out.set_value(reflect::DynamicObject(result));
    }
}

std::string Expression::serialize() const
{
    YAML::Node s;
    s.push_back(out.param().id());
    s.push_back(expr);

    s.SetStyle(YAML::EmitterStyle::Flow);
    YAML::Emitter out;

    auto tag = YAML::VerbatimTag("expr");
    out << tag << s;
    return out.c_str();
}

ExpressionPtr Expression::deserialize(
    YAML::Node const& node,
    FeatureContainer const& features
)
{
    std::string expr = node[1].as<std::string>();
    mup::ParserX p;
    p.EnableAutoCreateVar(true);
    p.SetExpr(expr);

    std::vector<DynamicFeature> input_vec;
    for (auto const& [input_name, var] : p.GetVar()) {
        auto feature_it = features.find(input_name);
        if (feature_it == std::end(features)) {
            throw io_error(
                "Could not find input "s
                    + input_name + " in expression " + node[0].as<std::string>()
                    + " = " + expr
                    + ". Are the steps in the correct topological order?"
            );
        }
        input_vec.push_back(feature_it->second);
        // cout << item->first << "=" << (Variable&)(*(item->second)) << "\n";
    }

    return grunk::expression("", expr, std::move(input_vec));
}

ExpressionPtr expression(std::string const& id, std::string const& expr, std::vector<DynamicFeature> const& args)
{
    return parametric::compute_node_ptr<Expression>(new Expression(id, expr, args));
}

} // namespace grunk