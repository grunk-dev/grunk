#pragma once 

namespace ns {

    template <typename T>
    struct ATemplate {};

    struct Foo {
        struct Bar {};

        Bar fun1(void*);
    };

    using Baz = Foo::Bar;

    double fun2(Foo*, Foo::Bar&, Baz const&);

    Baz& fun3();

    Baz fun4(Baz, Foo, Foo*);

    ATemplate<Baz> fun5(ATemplate<Baz>&);
}
