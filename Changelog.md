# v0.3.2

 - bump reflect to 0.2.0
 - added default constructors to double, int, bool and String in `grunk::init` (#112)
 - added conversion to and from bool (`.as_bool()`) in python bindings (#113)
 - enable -h and -v shorthands in CLI for --help and --version
 - fix bugs when creating environments: On Windows, the `lib` directory should be used as a source for copying dlls into the environment folder (#114)
 - Stop raising an exception, if a recipe's reported grunk version does not match the actual grunk version exactly. A more elaborate version checking still 
   remains to be done (#116)
 - Support storing metadata in yaml format in a recipe. The metadata of a recipe is a yaml map with a reserved keyworkd "grunk" for internal metadata 
   (currently unused). External tools can ammend their tool-specific metadata under another key to the metadata of a node (#117)
 - It is now possible to pass parameter names when registering free functions, member functions and constructors (#120)

# v0.3.1

 - bump reflect to 0.1.11
 - bump yaml-cpp to 0.8.0
 - New feature: grunk environments to manage dedicated directories with installed plugins in the user home (#111)
 - Remove grunk virtualenv feature. Grunk environments should be used instead (#111)
 - Don't automatically add `PATH`, `LD_LIBRARY_PATH` and `DYLD_LIBRARY_PATH` to the search path.
   The method `grunk::PluginRegistry::populate_path_form_env()` still exists, but is not called in `grunk::init()`
 - grunk plugin manager now installes directly into the local conan cache, instead of keeping a seperate cache (#111)
 - fix installation issue of python bindins: setup.py did not copy reflect.dll into site-packages/grunk (#108)
 - bugifx in code generator: No more superfluous namespace seperators at the beginning of declaration names (#107)

# v0.3.0

 - bump reflect to 0.1.10
 - bump parametric to 0.3.4
 - Fix compiler error on MSVC 17.9 related to lambdas (!64)
 - several fixes to code generator, specifically to generate portable code (not resolving typedefs) and supporting generated source code 
   that is seperated into one translation unit per module. This speeds up compiplation of grocc.
 - Allow loading and unloading of individual plugins with `grunk::PluginRegistry::load(std::string const& pluginName)`. 
   When the plugin registry is initialized, the search path is updated from `PATH`, `LD_LIBRARY_PATH` and `DYLD_LIBRARY_PATH`, which are
   populated with `grunk virtualrunenv` (#87)
 - Support serialization of static `Action<F>`. The function `F` must exist in the static function registry for this to work (#38)
 - Removed conversion of `DynamicFeature` to `Feature<T>`. This was inconsistent and sometimes forced evaluation of the DAG or added to the DAG.
   This feature is currently unused (#100, #101)
 - The functions `Feature::create`, `Feature::get` and `Feature::invoke` were removed. The same functionality can be achieved with an action 
   directly because constructors, data members and member functions are all stored as `reflect::DynamicFunction` instances (#98)
 - The `StdPlugin` class was removed, becuase it is not really a plugin. Initialization now takes place in `grunk::init` (#97)
 - several additions to the reflection of standard types in `grunk::init`
 - Introduce constants: Parameters with id "" will be considered constants. These will be serialized directly 
   in place of the arguments in the `steps` block rather than the `parameters` block of a recipe. For now, this 
   only works for actions, not yet for vec, script, expression and recipe. (#36)
 - Fix regression, void functions did not work anymore (#94)
 - Fix issue, where serialization did not always respect topological order (#91)
 - Fix issue, where two expressions could not be concatenated (#90)
 - Fix issue, where `Recipe::clone` did not copy the type descriptors of the `DynamicFeature`s (#89)
 - Fix serialization of functions without inputs, which were serialized into the parameters block previously (#88)
 - Fix assertion value when using the return value of a script in another action (#85)

# v0.2.2

 - bump reflect to 0.1.7
 - bump parametric to 0.3.0
 - New logo!
 - Implementation of `grunk::Recipe`. This allows calling recipes like compute nodes and modularizing a recipe via the call to subrecipes (#40)
 - Implementation of `grunk::Script` that allows calling a sequence of steps within a single compute node. The steps within the compute node are 
   not performed lazily and results are not cached. In contrast to a `grunk::Recipe`, steps are allowed to use functions that are not referentially 
   transparent, such as non-const getters or setters, so long as they are not called on inputs of the script (#70)
 - Implementation of `grunk::Vec` whic allows passing several `DynamicFeature` instances to a function expecting an `std::vector<T>` (#60).

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
