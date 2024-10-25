# Hello World

This example demonstrates how to build a model and evaluate it using the Python bindings. `build_model.py` creates a new very simple grunk recipe using the plugin grocc and exports
it to yaml. `run_model.py` reads this recipe, modifies a value and exports the resulting shape (just a box) both to step and to brep.

To get started, create a grunk environment named `cad` that includes the plugin `grocc/0.1.4`:

```
grunk env create cad grocc/0.1.4
```

Then run `build_model.py` to generate the grunk recipe:

```
python build_model.py 
```

This should create a file `box.grr.yml`. Now run `run_model.py` to read the recipe, modify an independent input feature and export a resulting shape to a CAD file:

```
python run_model.py
```