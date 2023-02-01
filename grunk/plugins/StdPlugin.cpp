#include <grunk/plugins/StdPlugin.hpp>
#include <grunk/version.hpp>
#include <yaml-cpp/yaml.h>

namespace grunk {

std::string StdPlugin::name() const
{
    return "";
}

std::string StdPlugin::version() const 
{
    return grunk_VERSION;
}

void StdPlugin::init() const
{
    register_type<bool>("bool");

    register_type<int>("int");
    
    register_type<double>("double")
    .add_constructor<double>()
    .add_member_function(
        [](double const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<double>(); },
        "deserialize"
    );

    register_type<std::string>("string");
    
    register_type<const char*>("cstring")
    .add_conversion<std::string>();
}

} //namespace grunk
