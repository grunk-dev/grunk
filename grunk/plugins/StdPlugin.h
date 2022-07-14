#pragma once

#include "IPlugin.h"

namespace grunk {


/**
 * @brief The grunk "standard plugin".
 *
 * This plugin is always loaded first. 
 * It registers C++ primitive types and functions
 *
 * @ingroup plugin
 * 
 */
class StdPlugin: public grunk::IPlugin
{
public:

    virtual std::string name() const override final;

    virtual void init() const override final;

};

} //namespace grunk