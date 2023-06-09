# v0.2.1

 - bump reflect to 0.1.6
 - Add `grunk::expression` and `grunk::Expression` to create 
 compute nodes, that represent the evaluation of simple calculations. 
 This implementation is based on muparser and all functions and operators supported by muparser can be used (#33). 


# v0.2.0

 - `DynamicFeature` constructor accepting `DynamicFeature`s as argument has been removed. This caused ambiguities with default-constructible types. Now there is a create factory method to create a new `DynamicFeature` from others. Also, `action` can be used to evaluate a constructor (#39)
 - Add a CLI subcommand to execute grunk recipes from the command line (#64)
 - Fix bug related to the fully qualified name of a `reflect::DynamicFunction`, when serializing a `DynamicAction` to yaml (#61)
 - Support an additional means of customizing the code generation: The 
   generation of registration code is moved to a `CodeGenerator` class, that can be subclassed. The subclassed class can be provided using a config file (#73)
 - Added functionality to automatically load and install plugins using the conan api under the hood (#66)
 - `DynamicFeature::get`, `Feature::get`, `DynamicFeature::invoke` and `Feature::invoke` have id as first parameter for consistency with `action` (#55)
 - Create CLI and move package manager and code generator from grunkan to grunk (#65)
 - Add python bindings using pybind11 (#63)

# v0.1.3

 - Avoid unnecessary copy of return value from `grunk::action` [#58](https://gitlab.dlr.de/paradigms/grunk/-/issues/58)
 - Add custom string proxy that can be converted to `std::string` and `const char*` [#59](https://gitlab.dlr.de/paradigms/grunk/-/issues/59)
 - bump reflect from 0.1.3 to 0.1.4

# v0.1.2

 - Update reflect to 0.1.3

# v0.1.1

 - prefix functions and types with plugin name
 - implement runtime overload resolution
