# Gas turbine cavity

![](cavity.png)

A cavity in the secondary air system of a gas turbine. This model uses only grocc functionality. 

Setup the environment with 

```console
grunk virtualrunenv grocc/0.1.1
```

This will generate `activate_run.sh` and similar files in the current directory. These files must be sourced before running any of the scripts, so that grunk, all plugins and all transitive runtime dependencies are in the search path. As a convenience, `build_model.sh` and `run_model.sh` are scripts that source `activate_run.sh` and then execute the python scripts.

To (re-)generate the yml-recipe enter:

```console
./build_model.sh
```

To open the YAML recipe, modify the model and save the output as brep enter:

```console
./run_model.sh
```