#pragma once 

namespace ns {

    struct Foo {
        struct Bar {};

        Bar fun1(void*);
    };

    using Baz = Foo::Bar;

    double fun2(Foo*, Foo::Bar&, Baz const&);

    Baz& fun3();

    Baz fun4(Baz, Foo, Foo*);
}
