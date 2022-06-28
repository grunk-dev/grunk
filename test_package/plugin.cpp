#include <grunk/plugins/api.h>

struct MyDouble {

    MyDouble(double v) : value(v) {}
    double value;
};

MyDouble add(MyDouble const& l, MyDouble const& r) {
    return MyDouble(l.value + r.value);
}

class Bar: public grunk::IPlugin
{
public:

    virtual std::string name() const override final
    {
        return "Bar";
    }

    virtual void init() const override final 
    {
        // register types

        grunk::register_type<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddDataMember(&MyDouble::value, "value");

        // register functions

        grunk::register_function(&add, "add", "adds two MyDouble values");
    }

};
GRUNK_REGISTER_PLUGIN(Bar)
