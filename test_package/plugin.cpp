#include <grunk/plugins/api.h>

struct MyDouble {

    MyDouble(double v) : value(v) {}

    void multiply(int factor) {
        value *= factor;
    }

    double value;
};

class Bar: public grunk::IPlugin
{
public:

    virtual std::string name() const override final
    {
        return "Bar";
    }

    virtual void register_types() const override final 
    {
        Reflect::Reflect<MyDouble>("MyDouble")
        .AddConstructor<double>()
        .AddMemberFunction(&MyDouble::multiply, "multiply")
        .AddDataMember(&MyDouble::value, "value");
    }

};
GRUNK_REGISTER_PLUGIN(Bar)
