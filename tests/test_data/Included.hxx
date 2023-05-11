#pragma once 

#define I_AM_A_MACRO_WITHOUT_TRAILING_SEMICOLON_MUAHAHA using type=int;

using Standard_Real = double;

namespace ns1 {

    class Other {
        // should not (automatically) be parsed recursively, when parsing Foo.hxx
    };

}

class ForwardDeclared {};

namespace ns2 {

    ns1::Other some_function(Standard_Real const* xyz);

}
