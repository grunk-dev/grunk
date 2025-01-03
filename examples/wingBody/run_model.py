import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load_env("dlrk")
    
    
    recipe = grunk.read("wing_body.grr.yml")
    nose = recipe["nose"].value()
    grunk.reflect.invoke("grocc::BRepTools::Write", nose, "nose_face.brep")
    grunk.reflect.invoke("grocc::export_to_step", nose, "nose_face.stp")
