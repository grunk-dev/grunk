# Source Code of grunk

The source code of grunk is organized as follows:

## grunk.h

This is a convenience header that exposes everything from the public API of
grunk.

## core

This directory contains the core of grunk, namely the implementation of the 
classes `Algorithm` and `Feature`. 

An `Algorithm` is a node in the feature 
tree representing a calculation. It connects input features and output features, which are instances of the `Feature` class.

A `Feature` is a node in the feature tree representing any kind of object. This could be a parameter (an independent root node) or a feature as a result of a computation using an instance of the `Algorithm` class.

## dynamic

This directory contains the implementation of grunk's dynamic typing system. It contains the implementation of `RuntimeFunction`s, which wrap normal functions but map `Reflect::DynamicObject`s as inputs to `DynamicObject`s.

The directory also contains implementations of `RuntimeAlgorithm` as a template specialization of an `Algorithm` wrapping a `RuntimeFunction` as well as `RuntimeFeature`, which is a template specialization of a `Feature` wrapping a `DynamicObject`.

Finally, it contains the implementation of the `FunctionRegistry`, which allows plugin authors to register functions with grunk and create `RuntimeAlgorithm` instances from it.

## io

This directory contains the implementation of File I/O, that is reading 
and writing to a parametric file in the JSON format.

## plugins

This directory contains the implementation of the plugin system. You can find the implementation of the `PluginRegistry`, the interface class `IPlugin` that is to be used by plugin authors and the `StdPlugin`, that is always loaded and registers C++ standard types and functions.