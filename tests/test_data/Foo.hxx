#pragma once
#include <string>
#include <vector>
#include <Included.hxx>

class ForwardDeclared;

class Baz {
public:
    // special operator, that is always static
    void* operator new(size_t);
};

namespace ns2 {

struct Bar {

    // macro without trailing semicolon, defined in included header
    I_AM_A_MACRO_WITHOUT_TRAILING_SEMICOLON_MUAHAHA

    Bar();

    int x;
    Standard_Real y; // datatype (typedef) from include

    void bar_fun();
};


class Foo : Baz, public Bar {
public:
    Foo() = default;
    Foo(Standard_Real x, Standard_Real y, Standard_Real z);
    explicit Foo(bool);
    Foo(Foo const&) = delete; // this should not be parsed as a ctor

    // conversion function
    operator ns1::Other() {
        return {};
    }

    double baz(int) const;     // overload for int
    double baz(double) const; // overload for double

    static void static_func(std::string const&);

    std::vector<ForwardDeclared> data_member;

private:

    void private_func(bool);

    Standard_Real x;
    Standard_Real y;
};


ns1::Other some_function(ForwardDeclared const& xyz, Bar* abd);

} // namespace ns2