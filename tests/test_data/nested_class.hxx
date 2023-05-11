#pragma once 

class Foo {
public:
    struct Bar {};
    enum Color { red, green, blue };
    enum class Boolean { yes, no };

    Bar baz(Color);
};