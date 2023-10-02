#include <grunk/dynamic/Script.hpp>
#include <initializer_list>

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
    std::initializer_list<std::string> const& outputs_,
    std::initializer_list<Argument> const& inputs
)
    : function_name(fun)
    , outputs(outputs_)
    , arguments(inputs)
{};

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
    std::initializer_list<Step> const& s,
    std::initializer_list<std::string> const& r
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
            }
            if (std::holds_alternative<int>(a)) {
                throw script_error("Step arguments for new scripts must either be strings or DynamicFeatures. integer arguments are for interal use only.")
            }
        }
    }
}

ResultHolder<DynamicAction> script(
    std::initializer_list<Script::Step> const& steps,
    std::initializer_list<std::string> const& returns    
)
{
    auto ptr = std::shared_ptr<Script>(new Script(steps, returns));
    return ResultHolder<DynamicAction>(
        parametric::compute(ptr, steps),
        *ptr
    );
}

void Script::connect_inputs(std::initializer_list<Step> const& steps_)
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