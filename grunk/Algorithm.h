#pragma once


#include <functional>
#include <parametric/core.hpp>
#include "RuntimeObject.h"

namespace grunk {

class Algorithm : public parametric::ComputeNode
{
public:
    using InputsVec = std::vector<std::reference_wrapper<RuntimeObject const>>;
    using OutputsVec = std::vector<RuntimeObject>;
    using Function = std::function<OutputsVec(InputsVec const&)>;

    Algorithm(Function fun, std::initializer_list<parametric::param<RuntimeObject>> const&);

    void eval() const override;

    std::vector<parametric::OutputParam<RuntimeObject>> const& get_outputs() const;

private:
    Function function;
    std::vector<parametric::param<RuntimeObject>> const inputs;
    std::vector<parametric::OutputParam<RuntimeObject>> mutable outputs;
};

} //namespace grunk