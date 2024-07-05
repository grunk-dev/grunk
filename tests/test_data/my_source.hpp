#pragma once 

template <typename T, int = 42>
struct NonTypeTemplateArg {};

namespace ns {

    template <typename T>
    struct ATemplate {
        using value_type = T;
    };

    struct Foo {
        struct Bar {};

        Bar fun1(void*);
    };

    using Baz = Foo::Bar;

    double fun2(Foo*, Foo::Bar&, Baz const&);

    Baz& fun3();

    Baz fun4(Baz, Foo, Foo*);

    typedef ATemplate<Baz> ABaz;

    ABaz::value_type fun5(ATemplate<Baz>&, ATemplate<Baz>);

    const char * fun6();

    auto fun7(NonTypeTemplateArg<ATemplate<Baz>, 5>);

}
