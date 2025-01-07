import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load_env("dlrk")
    
    
    recipe = grunk.read("wing_body.grr.yml")

    for key in ["nose", "central_fuselage", "tail_mantle", "tail_back", "fuselage_face"]:
        shape = recipe[key].value()
        grunk.reflect.invoke("grocc::BRepTools::Write", shape, f"{key}_face.brep")
        grunk.reflect.invoke("grocc::export_to_step", shape, f"{key}_face.stp")
