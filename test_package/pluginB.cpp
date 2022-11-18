#include <pluginA.hpp>
#include <grunk/grunk.hpp>
#include <iostream>

MyDouble multiply(MyDouble const& l, MyDouble const& r) {
    return {l.value * r.value};
}

class PluginB: public grunk::IPlugin
{
public:

    virtual std::string name() const override final
    {
        return "PluginB";
    }

    virtual std::string version() const override final
    {
        return "1.3.0";
    }

    virtual void init() const override final 
    {
        // register functions

        grunk::register_function(&multiply, "multiply");
    }

};
GRUNK_REGISTER_PLUGIN(PluginB)
