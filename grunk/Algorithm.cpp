#include "Algorithm.h"

#include <vector>

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
    // tranform input nodes to vector of runtime objects
    InputsVec inputs_vec;
    std::transform(inputs.begin(),
                   inputs.end(),
                   inputs_vec.begin(),
                   [](auto const& param) { return std::ref(param.value()); }
    );

    // call the wrapped function
    auto outputs_vals = function(inputs_vec);
    
    // move the output values to the output nodes
    for (int i=0; i<outputs.size(); ++i) {
        if (!outputs[i].expired()) {
                    outputs[i].set_value(std::move(outputs_vals[i]));
        }
    }
}

std::vector<parametric::OutputParam<RuntimeObject>> const& Algorithm::get_outputs() const
{
    return outputs;
}

} //namespace grunk