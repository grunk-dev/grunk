import grunk

# a dictionary of CPACS models with the indices of wing, component segment, 
# and the ribs and spars that are used to create the wingbox
cpacs_models = {
    "D150": {
        "cpacs_file": "CPACS_30_D150.xml", 
        "cpacs_config": "D150_VAMP",
        "wing_idx": 1,
        "cs_idx": 1,
        "spar_le_idx": 1,
        "spar_te_idx": 2,
        "ribs_def_root_idx": 1,
        "rib_root_idx": 1,
        "ribs_def_tip_idx": 7,
        "rib_tip_idx": 1
    },
    "codex_example": {
        "cpacs_file": "codex_example_aircraft.xml", 
        "cpacs_config": "AircraftModel",
        "wing_idx": 1,
        "cs_idx": 1,
        "spar_le_idx": 1,
        "spar_te_idx": 2,
        "ribs_def_root_idx": 1,
        "rib_root_idx": 1,
        "ribs_def_tip_idx": 38,
        "rib_tip_idx": 1
    },
}
fuel_system_recipes = {
    "D150" : "fuel_D150.grr",
    "codex_example": "fuelsystem.grr.yml"
}

if __name__ == '__main__':


    #############################################################
    # Initialize grunk and load plugins from environment "tigl" #
    #############################################################

    grunk.init()
    grunk.get_plugin_registry().load_env("tigl")

    ########################
    # Select configuration #
    ########################

    config = "D150"
    # config = "codex_example"

    #######################################################
    # Read recipe for wingbox creation and set parameters #
    #######################################################

    wingbox_recipe = grunk.read("CPACS_wingbox.grr.yml")

    # set the input parameters to the wingbox recipe
    for (key, value) in cpacs_models[config].items():
        wingbox_recipe[key].set_value(value)

    ######################################################
    # Export some features from the recipe as brep files #
    ######################################################

    # export some result features as breps
    for id in [
        "fused_shape",
        "wing_shape", 
        "spar_le_shape", 
        "spar_te_shape", 
        "rib_root_shape", 
        "rib_tip_shape", 
        "split4",
        "wing_le_tool"
        ]:
        feature = wingbox_recipe[id]
        grunk.reflect.invoke("grocc::BRepTools::Write", feature.value(), id + ".brep")

    #############################################################
    # Evaluate the fuel system recipe and export result as brep #
    #############################################################

    fuelsystem_recipe = grunk.read(fuel_system_recipes[config])
    fuel_system = fuelsystem_recipe["compound"].value()
    grunk.reflect.invoke("grocc::BRepTools::Write", fuel_system, "fuel_system.brep")

    #########################
    # Merge the two recipes #
    #########################

    recipe =  grunk.Recipe()
    recipe.insert_recipe("wingbox", wingbox_recipe)
    recipe.insert_recipe("fuelsystem", fuelsystem_recipe)
    #TODO define parameters in root recipe and map to subrecipe in evaulation?
    grunk.write("CPACS_wingbox_fuelsystem.grr.yml", recipe)