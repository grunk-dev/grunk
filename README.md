<!--
SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>

SPDX-License-Identifier: MPL-2.0
-->

![](docs/images/grunk_logo.png)

**share tools - share designs - build together**

[![CI](https://github.com/grunk-dev/grunk/actions/workflows/ci.yml/badge.svg)](https://github.com/grunk-dev/grunk/actions/workflows/ci.yml)
[![documentation](https://img.shields.io/badge/docs-online-blue)](https://grunk-paradigms-fa948b0b9b25e1a01f6c9cf07a0b575274b57f414302a5d.pages.gitlab.dlr.de/)

**Disclaimer:** *This is work in progress at a very early stage. Most features have not been implemented yet. Expect the code and API to change frequently!*

Grunk is a C++ library for dataflow/incremental programming.Use it to create generic parametric modelings using any C++ type or function as building blocks. 

 - grunk is targeted at - but not limited to - geometric modeling
 - header-only C++17 library with Python bindings
 - Dynamic typing and scripting interface based on Lua
 - Reproducible exchange file format based on mixed YAML and Lua
 - Plugin-interface to add custom types and functions as building blocks at runtime
 - Package manager for grunk plugins *(WIP)*

Models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.

## Library overview

| Library | Description | Dependencies |
| -- | -- | -- |
| `grunk::core` | Backend for tracking parametric dependencies. Lazy evaluation and automatic invalidation with minimal overhead. Header-only, thin wrapper for parametric | [parametric](https://github.com/grunk-dev/parametric), *optional: [taskflow](https://github.com/taskflow/taskflow) for multithreading support. Build with `GRUNK_WITH_TASKFLOW=ON` cmake option/precompiler definition to enable multithreading.* |
| `grunk::dynamic` | Dynamic scripting support (Lua). Write scripts without boilerplate that are automatically parametric. Simple serialization and deserialization of parametric trees to and from Lua. Header-only. | `grunk::core`, [Lua](https://www.lua.org/), [sol2](https://github.com/ThePhd/sol2) |
| `grunk::recipe` | YAML-based recipes: A human-readable structured exchange format for parametric models | `grunk::dynamic`, [yaml-cpp](https://github.com/jbeder/yaml-cpp) | 
| `grunk::plugin` | Plugin support for sharing re-usable functions and data types. Plugins can be written in C++ for maximum performance, as a compiled Lua module (e.g. via SWIG), or in plain Lua for ease-of-use | `grunk::dynamic` |

## Sneak Peak

### grunk::parametric

In grunk, features are data nodes and actions are calculation nodes that are connected in an acyclic graph. 

grunk comes with a static and dynamic mode, where the static mode does not entail the small overhead of the dynamic type system.

The following example shows the core principle behind lazy evaluation and caching. It also shows how static mode and dynamic mode can be mixed *(dynamic mode requires  `grunk::dynamic`)*:

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
    auto b = grunk::feature(2);          // type of b: Feature<int>
    auto c = grunk.feature(3);           // type of c: Feature<object>
    auto d = grunk.action(add, a, b);    // type of d: Feature<int>
    auto e = grunk.action("add", c, d);  // type of e: Feature<object>

    // Until here, add has not been called, but only the DAG assembled, 
    // that represents the dependency of the features. Now let's trigger
    // evaluation by querying the value of e.

    assert(e.value().as<int>() == 6);

    // After evaluation all results, including intermediate results are
    // cached. A second query of e would just retrieve the value from
    // cache

    // resetting an independent input feature invalidates all dependent
    // nodes. Resetting c will invalidate e, but the cache of d remains
    // valid
    c.set_value(4);

    // A new query of e will trigger evaluation of all invalid nodes.
    assert(e.value().as<int> == 7);

    return 0;
}
```

### grunk::dynamic

The same model can be built using the Lua scripting interface enterily in dynamic mode.

```cpp
#include <grunk/state.hpp>

int main() {

    grunk::state grunk;

    // register a function in the dynamic grunk state
    auto add = [](int l, int r){ return l+r; };
    grunk.register_function("add", add);

    env = grunk.create_env();
    env.eval(R"(
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

Note, that for convenience, grunk provides python bindings and mathematical operators.

```python
import grunk

# ...

env = grunk.create_env()
env.eval("""
  local a = grunk.feature(1)
  local b = grunk.feature(2)
  local c = grunk.feature(3)
  local d = a + b
  local e = c + d

  e1 = e:value()
  c:set_value(4)
  e2 = e:value()
""")

assert grunk.get("e1").as_int() == 6
assert grunk.get("e2").as_int() == 7

```

Function and type registration in the dynamic scripting backend only works in C++ or Lua. Typically functions and types are imported as part of a grunk runtime plugin.

### grunk::recipe

The parametric tree constructed above can be written to and read from a *grunk recipe* with the following contents:

```yaml
uses:
  grunk: 0.5.0
  # additionally used plugins here, if any
parameters:
  a: 1
  b: 2
  c: 3
steps: |
  d = a + b
  e = c + d
```

### grunk::plugin

Any type and any function can be registered in the dynamic type system of grunk in a runtime plugin. Using plugins provides the possibility to create, share and reuse parametric models. A plugin reports a name and version and registers everything it provides under a namespace table keyed by its own name - grunk's dynamic scripting engine treats that namespace exactly like any other, so a recipe never needs to know or care which kind of plugin filled it in.

A plugin written in C++ gets a namespace to register into via `grunk::state::begin_plugin`, and exports two fixed entry points so `grunk::plugin::load_native` can load it from a shared library at runtime without knowing its name up front:

```cpp
// my_plugin.cpp - built as its own shared library, e.g. my_plugin.so
#include <grunk/grunk.hpp>

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"my_plugin", "1.0.0"};
}

GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info)
{
    auto ns = state.begin_plugin(info);
    state.register_function(
        "add",
        [](int l, int r){ return l + r; },
        {},
        ns,
        info.name // qualifies "add"'s serialized name as "my_plugin.add" - ns already carries
                  // this name itself, so it's auto-inferred if left out; passing it explicitly,
                  // as above, is still fine and documents intent at the call site.
    );
}
```

`GRUNK_PLUGIN_EXPORT` (not a plain `extern "C"`) is what makes this portable: it expands to
`extern "C" __declspec(dllexport)` on Windows, where a DLL exports nothing by default, and to
plain `extern "C"` on Linux/macOS, where a shared library already does.

Loading a plugin under a name that's already loaded on the same `grunk::state` throws rather than
silently discarding the first plugin's namespace table - call `state.clear_module(name)` first if a
reload is genuinely intended.

```cpp
#include <grunk/grunk.hpp>
#include <grunk/plugin.hpp>

int main() {
    grunk::state grunk;
    grunk::plugin::load_native(grunk, "my_plugin.so");

    auto env = grunk.create_env();
    env.eval("c = my_plugin.add(1, 2)");
    assert(env.get("c").as<int>() == 3);

    return 0;
}
```

A plugin can just as well be a compiled Lua module (e.g. SWIG-generated) via `grunk::state::load_compiled_plugin`, or plain Lua source via `grunk::state::load_lua_plugin_script`/`load_lua_plugin_file` (or `grunk::plugin::load_script`) - see `grunk/plugin/loader.hpp` for the full native ABI and `grunk/dynamic/state.hpp` for all three `load_*_plugin`/`begin_plugin` methods.

## Documentation

[Read the documentation](https://paradigms.pages.gitlab.dlr.de/grunk/) to learn more.

## Examples

Grunk includes example applications demonstrating the library's capabilities. Each example is standalone with its own dependencies and build configuration.

### Available Examples

| Example | Description |
| -- | -- |
| [examples/cpp/cad_autodiff](examples/cpp/cad_autodiff/) | CAD modeling with automatic differentiation using geoml |

### Building Examples

```bash
# CAD Autodiff example
cd examples/cpp/cad_autodiff
pixi run all
```

See individual example README files for details.

## License

This project is licensed under the Mozilla Public License 2.0 - see the [license file](LICENSES/MPL-2.0.txt) file for details.
