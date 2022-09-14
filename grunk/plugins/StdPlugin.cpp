#include <grunk/plugins/StdPlugin.h>
#include <grunk/version.h>

namespace grunk {

std::string StdPlugin::name() const
{
    return "Std";
}

std::string StdPlugin::version() const 
{
    return grunk_VERSION;
}

void StdPlugin::init() const
{
    register_type<bool>("bool");

    register_type<int>("int");
    
    register_type<double>("double");

    register_type<std::string>("string");
    
    register_type<const char*>("cstring")
    .AddConversion<std::string>();
}

} //namespace grunk