#pragma once

#include "IPlugin.h"

namespace grunk {


// The grunk "standard library plugin" reflects primitive types etc
class StdPlugin: public grunk::IPlugin
{
public:

    virtual std::string name() const override final;

    virtual void init() const override final;

};

} //namespace grunk