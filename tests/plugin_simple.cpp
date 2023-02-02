#include <grunk/grunk.hpp>

struct MyDouble {

    MyDouble(double v) : value(v) {}
    double value;
};

MyDouble add(MyDouble const& l, MyDouble const& r) {
    return MyDouble(l.value + r.value);
}

class SimplePlugin: public grunk::IPlugin
{
public:

    virtual std::string name() const override final
    {
        return "SimplePlugin";
    }

    virtual std::string version() const override final
    {
        return "1.2.3";
    }

    virtual void init() const override final 
    {
        // register types

        register_type<MyDouble>("MyDouble")
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

        register_function(&add, "add");
    }

};
GRUNK_REGISTER_PLUGIN(SimplePlugin)
