#include <grunk/dynamic/Script.hpp>
#include <vector>

using namespace std::string_literals;

namespace grunk {

script_error::script_error(std::string const& msg)
 : mMessage("grunk script error: "s + msg)
{}

const char* script_error::what() const noexcept
{
    return mMessage.c_str();
}

std::string script_error::get_message() const
{
    return mMessage;
}

Script::Step::Step(
    std::string fun,
    std::vector<std::string> const& outputs_,
    std::vector<Argument> const& inputs
)
    : function_name(fun)
    , outputs(outputs_)
    , arguments(inputs)
{};

YAML::Node Script::serialize(Script::Step const& step) const 
{
    YAML::Node s;
    
    YAML::Node o;//(YAML::NodeType::Sequence);
    for (auto const& output: step.outputs) {
        o.push_back(output);
    }
    s.push_back(o);

    YAML::Node i;//(YAML::NodeType::Sequence);
    for (auto const& a : step.arguments) {
        if (std::holds_alternative<std::string>(a)) {
            i.push_back(std::get<std::string>(a));
        } else if (std::holds_alternative<DynamicFeature>(a)) {
            i.push_back(std::get<DynamicFeature>(a).id());
        } else {
            i.push_back(this->arg<reflect::DynamicObject>(std::get<int>(a)).id());
        }
    }
    s.push_back(i);

    s.SetTag(step.function_name);
    s.SetStyle(YAML::EmitterStyle::Flow);

    return s;
}

std::string Script::serialize() const
{
    YAML::Node r;

    YAML::Node sn;
    for (auto const& s : steps) {
        sn.push_back(serialize(s));
    }
    r["steps"] = sn;

    YAML::Node o;
    for (auto const& out : returns) {
        o.push_back(out);
    }
    if (o.size() > 0) {
        r["returns"] = o;
    }

    r.SetStyle(YAML::EmitterStyle::Block);
    YAML::Emitter out;
    out << YAML::VerbatimTag("script") << r;
    return out.c_str();
}

Script::Step Script::Step::deserialize(YAML::Node const& node, Recipe::FeatureContainer& features)
{
    auto function_name = node.Tag();
    std::vector<std::string> outputs;
    if (node[0]) {
        for (auto const& o : node[0]) {
            outputs.push_back(o.as<std::string>());
        }
    }

    std::vector<Script::Step::Argument> inputs;
    if (node[1]) {
        for (auto const& i : node[1]) {
            auto id = i.as<std::string>();
            if (auto search = features.find(id); search != features.end()) {
                inputs.push_back(search->second);
            } else {
                inputs.push_back(id);
            }
        }
    }
    return Script::Step(function_name, outputs, inputs);
}

ResultHolder<DynamicAction> Script::deserialize(YAML::Node const& node, Recipe::FeatureContainer& features)
{
    std::vector<Script::Step> steps_list;
    if (node["steps"]) {
        for (auto const& step : node["steps"]) {
            steps_list.push_back(Script::Step::deserialize(step, features));
        }
    }

    std::vector<std::string> output_list;
    if (node["returns"]) {
        for (auto const& o : node["returns"]) {
            output_list.push_back(o.as<std::string>());
        }
    }

    return script(steps_list, output_list);
}

void Script::eval(Script::Step const& s, Script::VariableMap& vars) const
{
    std::vector<reflect::DynamicObject> inputs;
    std::transform(
        s.arguments.cbegin(),
        s.arguments.cend(),
        std::back_inserter(inputs),
        [&](Script::Step::Argument const& a){
            if ( std::holds_alternative<std::string>(a)) {
                std::string as = std::get<std::string>(a);
                if (auto search = vars.find(as); search != vars.end()) {
                    return search->second;
                } else {
                    throw script_error("Could not resolve input variable \""s + as + "\".");
                }
            } else if ( std::holds_alternative<int>(a)) {
                return this->arg<reflect::DynamicObject>(std::get<int>(a)).value();
            } else {
                throw script_error("Step is not properly connected to script inputs.");
            }
        }
    );

    auto const& overload = reflect::resolve_function(s.function_name);
    auto ret = overload.invoke(inputs);

    assert(ret.size() == s.outputs.size());

    for (size_t i=0; i < ret.size() ; ++i) {
        if (s.outputs[i] != "_") {
            vars[s.outputs[i]] = ret[i];
        }
    }
}

Script::Script(
    std::vector<Step> const& s,
    std::vector<std::string> const& r
)
 : steps(s) 
 , returns(r)
{
    // enumerate the arguments
    int i = 0;
    for (auto& s : steps) {
        for (auto& a : s.arguments) {
            if (std::holds_alternative<DynamicFeature>(a)) {
                a = i++;
            } else if (std::holds_alternative<int>(a)) {
                throw script_error("Step arguments for new scripts must either be strings or DynamicFeatures. integer arguments are for interal use only.");
            }
        }
    }
}

ResultHolder<DynamicAction> script(
    std::vector<Script::Step> const& steps,
    std::vector<std::string> const& returns
)
{
    auto ptr = std::shared_ptr<Script>(new Script(steps, returns));
    return ResultHolder<DynamicAction>(
        parametric::compute(ptr, steps),
        *ptr
    );
}

void Script::connect_inputs(std::vector<Step> const& steps_)
{
    for (auto s : steps_) {
        for (auto const& a : s.arguments) {
            if (std::holds_alternative<DynamicFeature>(a)) {
                depends_on(std::get<DynamicFeature>(a).param());
            }
        }
    }
}

Script::ResultType Script::initialize_results() const
{
    return Script::ResultType(
        returns.size(), 
        DynamicFeature(parametric::new_param<reflect::DynamicObject>())
    );
}

void Script::connect_results(Script::ResultType const& res) {
    for (auto const& f : res) {
        computes(f.param());
    }
}

void Script::eval() const
{
    VariableMap vars;
    for (auto const& s : steps) {
        eval(s, vars);
    }

    for (size_t i=0; i< returns.size(); ++i) {
        if (auto out = this->template res<reflect::DynamicObject>(i); out) {
            out->set_value(vars.at(returns[i]));
        }
    }
}

void Script::post_connect() const 
{
    for (size_t i=0; i < returns.size(); ++i) {
        if (auto out = this->template res<reflect::DynamicObject>(i); out) {
            out->set_id(returns[i]);
        }
    }
}

} // namespace grunk
