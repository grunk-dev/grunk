#include "StdPlugin.h"

namespace grunk {

std::string StdPlugin::name() const
    {
        return "Std";
    }

void StdPlugin::init() const
{
    RegisterType<bool>("bool");

    RegisterType<int>("int");
    
    RegisterType<double>("double");

    RegisterType<std::string>("string");
    
    RegisterType<const char*>("cstring")
    .AddConversion<std::string>();
}

} //namespace grunk