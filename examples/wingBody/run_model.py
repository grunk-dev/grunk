import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load_env("wingbody")
    
    
    recipe = grunk.read("wing_body.grr.yml")

    for key in ["wingbodyref"]:
        print(key)
        shape = recipe[key].value()
        grunk.reflect.invoke("grocc::BRepTools::Write", shape, f"{key}.brep")
        grunk.reflect.invoke("grocc::export_to_step", shape, f"{key}.stp")
