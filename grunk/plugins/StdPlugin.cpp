#include <grunk/plugins/StdPlugin.hpp>
#include <grunk/version.hpp>
#include <grunk/helper/String.hpp>
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
    register_type<bool>("bool")
    .add_constructor<bool>()
    .add_conversion<int>()
    .add_member_function(
        [](bool const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<bool>(); },
        "deserialize"
    );

    register_type<int>("int")
    .add_constructor<int>()
    .add_conversion<double>()
    .add_member_function(
        [](int const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<int>(); },
        "deserialize"
    );
    
    register_type<double>("double")
    .add_constructor<double>()
    .add_constructor<int>()
    .add_conversion<int>()
    .add_member_function(
        [](double const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<double>(); },
        "deserialize"
    );

    register_type<helper::String>("String")
    .add_constructor<>()
    .add_constructor<std::string_view>()
    .add_constructor<const char*>()
    .add_conversion<const char*>()
    .add_conversion<std::string>()
    .add_member_function(
        [](helper::String const& str){
            return YAML::Node(static_cast<std::string>(str));
        },
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& node){
            return helper::String(
                node.as<std::string>()
            );
        },
        "deserialize"
    );
}

} //namespace grunk
