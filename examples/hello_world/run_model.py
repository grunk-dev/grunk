import grunk

if __name__ == '__main__':

    grunk.init()
    grunk.get_plugin_registry().load_env("cad")
    
    recipe = grunk.read("box.grr.yml")
    
    recipe["w"].set_value(12)
	
    grunk.reflect.invoke("grocc::BRepTools::Write", recipe["box"].value(), "cavity.brep")
    grunk.reflect.invoke("grocc::export_to_step", recipe["box"].value(), "cavity.stp")
    