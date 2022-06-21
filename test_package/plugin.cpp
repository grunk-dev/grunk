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

        grunk::RegisterType<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddDataMember(&MyDouble::value, "value");

        // register functions

        grunk::RegisterFunction("add", &add, "adds two MyDouble values");
    }

};
GRUNK_REGISTER_PLUGIN(Bar)
