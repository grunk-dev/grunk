#include "Expression.hpp"

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
    computes(output, parametric::param<reflect::DynamicObject>(id));
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
    if (!output.expired()) {
        output.set_value(reflect::DynamicObject(result));
    }
}

std::string Expression::serialize() const
{
    YAML::Node s;
    s.push_back(output.param().id());
    s.push_back(expr);

    s.SetStyle(YAML::EmitterStyle::Flow);
    YAML::Emitter out;

    auto tag = YAML::VerbatimTag("expr");
    out << tag << s;
    return out.c_str();
}

} // namespace grunk