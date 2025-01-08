import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load_env("wingbody")
    
    
    recipe = grunk.read("wing_body.grr.yml")

    for key in ["fuselage_shell", "wing_tip_cap", "wing_solid_split", "fuselage_shell_cut"]:
        print(key)
        shape = recipe[key].value()
        grunk.reflect.invoke("grocc::BRepTools::Write", shape, f"{key}.brep")
        grunk.reflect.invoke("grocc::export_to_step", shape, f"{key}.stp")
