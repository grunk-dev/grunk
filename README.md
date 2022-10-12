# Grunk


**share tools - share designs - build together**

[![pipeline status]( https://gitlab.dlr.de/paradigms/grunk/badges/main/pipeline.svg)](https://gitlab.dlr.de/paradigms/grunk/-/commits/main/)
[![documentation](https://img.shields.io/badge/docs-online-blue)](https://paradigms.pages.gitlab.dlr.de/grunk/)

**Disclaimer:** *This is work in progress at a very early stage. Most features have not been implemented yet. Expect the code and API to change frequently!*

Grunk is a parametric modeling engine that helps you build complex models from parameters, track the dependencies of your model features and annotate them with metadata. Features are evaluated lazily and they will be automatically invalidated if any of the features and parameters it depends on changes. You can write your model to a human-readable file to disk and rebuild your model from the saved file. Grunk is targeted at - but not limited to - geometric modeling.

The only assumption grunk makes is that the model consists of certain **features** *(objects of a specific type)* which can be computed with the help of given **functions** from other features or parameters. These functions are expected to be [referentially transparent](https://en.wikipedia.org/wiki/Referential_transparency) *(disregarding logging etc.)*. A model can therefore be identified with a *directed acyclic graph* of functions, their inputs and their outputs.

Functions and feature types are loaded at runtime from grunk plugins. By doing this, each grunk plugin provides some domain specific building blocks for any kind of model.

This way, models built with grunk are highly modular and extendable. Users can share plugins and enrich their models without the need to re-compile anything.

[Read the documentation](https://paradigms.pages.gitlab.dlr.de/grunk/) to learn more.


## Installation

See the [installation section](https://paradigms.pages.gitlab.dlr.de/grunk/installation.html#installation) of the documentation.

## Building grunk from source

Refer to the [build instructions](https://paradigms.pages.gitlab.dlr.de/grunk/installation.html#building-from-source) of the documentation.