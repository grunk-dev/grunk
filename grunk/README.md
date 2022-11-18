# Source Code of grunk

The source code of grunk is organized as follows:

## grunk.h

This is a convenience header that exposes everything from the public API of grunk.

## core

This directory contains the core of grunk, namely the implementation of the 
classes `Action` and `Feature`. 

An `Action` is a node in the feature 
tree representing a calculation. It connects input features and output features, which are instances of the `Feature` class.

A `Feature` is a node in the feature tree representing any kind of object. This could be a parameter (an independent root node) or a feature as a result of a computation using an instance of the `Action` class.

## dynamic

This directory contains specializations of `Action` and `Feature`, when 
used together with grunk's dynamic typing system based on the `reflect` 
library. 

## io

This directory contains the implementation of File I/O. Specifically, the functions `grunk::read` and `grunk::write` are implemented here.

## plugins

This directory contains the implementation of the plugin system. You can find the implementation of the `PluginRegistry`, the interface class `IPlugin` that is to be used by plugin authors and the `StdPlugin`, that is always loaded and registers C++ standard types and functions.