#include "Algorithm.h"

#include <vector>

namespace grunk {

void Algorithm::eval() const
{
    // tranform input nodes to vector of runtime objects
    InputsVec inputs_vec;
    std::transform(inputs.begin(),
                   inputs.end(),
                   std::back_inserter(inputs_vec),
                   [](auto const& param) { return std::ref(param.value()); }
    );

    // call the wrapped function
    auto outputs_vals = function(inputs_vec);

    assert(outputs_vals.size() == outputs.size());
    
    // move the output values to the output nodes
    for (int i=0; i<outputs.size(); ++i) {
        if (!outputs[i].expired()) {
                    outputs[i].set_value(std::move(outputs_vals[i]));
        }
    }
}

parametric::param<RuntimeObject> Algorithm::get(size_t idx) const
{
    return outputs[idx];
}

} //namespace grunk