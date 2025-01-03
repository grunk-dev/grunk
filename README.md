![](docs/images/grunk_logo.png)

**share tools - share designs - build together**

[![pipeline status]( https://gitlab.dlr.de/paradigms/grunk/badges/main/pipeline.svg)](https://gitlab.dlr.de/paradigms/grunk/-/commits/main/)
[![documentation](https://img.shields.io/badge/docs-online-blue)](https://grunk-paradigms-fa948b0b9b25e1a01f6c9cf07a0b575274b57f414302a5d.pages.gitlab.dlr.de/)

**Disclaimer:** *This is work in progress at a very early stage. Most features have not been implemented yet. Expect the code and API to change frequently!*

Grunk is a C++ library for dataflow/incremental programming.Use it to create generic parametric modelings using any C++ type or function as building blocks. 

 - grunk is targeted at - but not limited to - geometric modeling
 - header-only C++17 library with Python bindings
 - Dynamic typing and scripting interface based on LUA
 - Plugin-interface to add custom types and functions as building blocks at runtime *(WIP)*
 - Package manager for grunk plugins *(WIP)*
 - Reproducible exchange file format *(WIP)*

Models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.

## Sneak Peak

grunk comes with a static and dynamic mode, where the static mode does not entail the small overhead of the dynamic type system. 

```cpp
#include <grunk/state.hpp>

int main()
{
    grunk::state grunk;

    // register a function in the dynamic grunk state
    auto add = [](int l, int r){ return l+r; };
    grunk.register_function("add", add);

    /* create the following DAG (top to bottom):
            a   b
            |  / 
         c  d
         | /
         e
    */

    auto a = grunk::feature(1);          // type of a: Feature<int>
    auto b = grunk.feature(2);           // type of b: Feature<object>
    auto c = grunk.feature(3);           // type of c: Feature<object>
    auto d = grunk::action(add, a, b);   // type of d: Feature<int>
    auto e = grunk.action("add", c, d);  // type of e: Feature<object>

    // Until here, add has not been called, but only the DAG assembled, 
    // that represents the dependency of the features. Now let's trigger
    // evaluation by querying the value of e.

    assert(e.value() == 6);

    // After evaluation all results, including intermediate results are
    // cached. A second query of e would just retrieve the value from
    // cache

    // resetting an independent input feature invalidates all dependent
    // nodes. Resetting c will invalidate e, but the cache of d remains
    // valid
    c.set_value(4);

    // A new query of e will trigger evaluation of all invalid nodes.
    assert(e.value() == 7);

    return 0;
}
```

The same model can be built using the LUA scripting interface enterily in dynamic mode.

```cpp
#include <grunk/state.hpp>

int main() {

    grunk::state grunk;

    // register a function in the dynamic grunk state
    auto add = [](int l, int r){ return l+r; };
    grunk.register_function("add", add);

    grunk.eval(R"(
        local a = grunk.feature(1)
        local b = grunk.feature(2)
        local c = grunk.feature(3)
        local d = add(a,b)
        local e = add(c,d)

        e1 = e:value()
        c:set_value(4)
        e2 = e:value()
    )");

    assert(grunk.get("e1").as<int>() == 6);
    assert(grunk.get("e2").as<int>() == 7);

    return 0;
}
```

## Documentation

[Read the documentation](https://paradigms.pages.gitlab.dlr.de/grunk/) to learn more.


## Installation

The C++ library can be installed via conan or built from source using cmake, see the [installation section](https://paradigms.pages.gitlab.dlr.de/grunk/installation.html#installation) of the documentation for details.

The python module can be installed using `pip`:

```console
pip install git+https://gitlab.dlr.de/paradigms/grunk
```

Make sure you have [setup conan properly](https://paradigms.pages.gitlab.dlr.de/grunk/installation.html#setup-conan) before so that the dependencies can be downloaded during the installation with pip.

## Building grunk from source

Refer to the [build instructions](https://paradigms.pages.gitlab.dlr.de/grunk/installation.html#building-from-source) of the documentation.