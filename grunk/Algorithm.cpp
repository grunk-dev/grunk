#include "Algorithm.h"

#include <vector>
#include <range/v3/all.hpp>

using namespace ranges;

namespace grunk {

Algorithm::Algorithm(Function fun, std::initializer_list<parametric::param<RuntimeObject>> const& in)
 : function(fun)
 , inputs(in)
{
    for (auto& i: inputs){
        depends_on(i);
    }
    for (auto& o: outputs){
        computes(o, parametric::param<RuntimeObject>(""));
    }
}

void Algorithm::eval() const
{

    // tranform inputs to vector of runtime objects and call the wrapped function and 
    auto outputs_vals = function( 
        inputs | views::transform([](auto const& param) { return std::ref(param.value()); })
               | to<std::vector>()
    );
    
    for (auto&& [i, output] : outputs_vals | views::enumerate  ) {
        if (!outputs[i].expired()) {
                    outputs[i].set_value(std::move(output));
        }
    }
}

std::vector<parametric::OutputParam<RuntimeObject>> const& Algorithm::get_outputs() const
{
    return outputs;
}

} //namespace grunk