#include <reflect/reflect.hpp>
#include <grunk/common/init.hpp>
#include <grunk/common/String.hpp>
#include <yaml-cpp/yaml.h>

namespace grunk {

void init()
{
    reflect::register_type<bool>("bool")
    .add_constructor<>()
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
	
	reflect::register_type<size_t>("size_t")
	.add_constructor<>()
	.add_constructor<size_t>()
	.add_member_function(
        [](size_t const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<size_t>(); },
        "deserialize"
    );

    reflect::register_type<int>("int")
    .add_constructor<>()
    .add_constructor<int>()
	.add_conversion<size_t>()
    .add_conversion<double>()
    .add_conversion<bool>()
    .add_member_function(
        [](int const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<int>(); },
        "deserialize"
    );
    
    reflect::register_type<double>("double")
    .add_constructor<>()
    .add_constructor<double>()
    .add_constructor([](int x){ return double(x); }) // construct from int, avoid narrowing conversion error in MSVC
    .add_conversion<int>()
    .add_member_function(
        [](double const& v){ return YAML::Node(v); }, 
        "serialize"
    )
    .add_member_function(
        [](YAML::Node const& y){ return y.as<double>(); },
        "deserialize"
    );

    reflect::register_type<helper::String>("String")
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
