#include "pluginA.hpp"
#include <grunk/grunk.hpp>
#include <iostream>


MyDouble::MyDouble(double v) : value(v) {}


MyDouble add(MyDouble const& l, MyDouble const& r) {
    return {l.value + r.value};
}

class PluginA: public grunk::IPlugin
{
public:

    virtual std::string name() const override final
    {
        return "PluginA";
    }

    virtual std::string version() const override final
    {
        return "2.4.19";
    }

    virtual void init() const override final 
    {
        // register types

        grunk::register_type<MyDouble>("MyDouble")
        .add_constructor<double>()
        .add_data_member(&MyDouble::value, "value")
        .add_member_function(
            [](MyDouble const& d){
                YAML::Node out(d.value);
                return out;
            },
            "serialize"
        )
        .add_member_function(
            [](YAML::Node const& y){
                return MyDouble(y.as<double>());
            },
            "deserialize"
        );

        // register functions

        grunk::register_function(&add, "add", "adds two MyDouble instances");

    }

};
GRUNK_REGISTER_PLUGIN(PluginA)
