#include "StdPlugin.h"

namespace grunk {

std::string StdPlugin::name() const
    {
        return "Std";
    }

void StdPlugin::register_types() const
{
    Reflect::Reflect<bool>("bool");

    Reflect::Reflect<int>("int");
    
    Reflect::Reflect<double>("double");

    Reflect::Reflect<std::string>("string");
    
    Reflect::Reflect<const char*>("cstring")
    .AddConversion<std::string>();
}

} //namespace grunk