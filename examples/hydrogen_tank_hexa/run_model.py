import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load_env("test")
    
    
    recipe = grunk.read("tank_model.grr.yml")
    tank_geometry = recipe["tank"].value()
    grunk.reflect.invoke("grocc::BRepTools::Write", tank_geometry, "tank.brep")
    grunk.reflect.invoke("grocc::export_to_step", tank_geometry, "tank.stp")
