import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load("grocc")

    cavity_r = grunk.read("cavity_model.grr.yml")
    cyl1_height = cavity_r["cyl1_height"]
    cavity = cavity_r["cavity"]
    
    print(f"chaning cyl1_height from {cyl1_height.value().as_float()} to 40.")
    cyl1_height.set_value(40)
    
    #grunk.reflect.invoke("grocc::BRepTools::Write", cavity.value(), "cavity.brep")
    grunk.reflect.invoke("grocc::export_to_step", cavity.value(), "cavity.stp")