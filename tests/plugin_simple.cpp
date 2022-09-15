#include <grunk/grunk.h>

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

        grunk::register_type<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddDataMember(&MyDouble::value, "value")
        .AddMemberFunction(
            [](MyDouble const& d){
                YAML::Node out(d.value);
                return out;
            },
            "serialize"
        );

        // register functions

        grunk::register_function(&add, "add");
    }

};
GRUNK_REGISTER_PLUGIN(SimplePlugin)