import grunk

if __name__ == '__main__':
    grunk.init()
    grunk.get_plugin_registry().load_env("cad")

    cavity_r = grunk.read("20240912/20240912.grr.yml")
    cavity = cavity_r["cavity"] # "revol" instead of "cavity" because the cylinders are initially left out
       
    grunk.reflect.invoke("grocc::BRepTools::Write", cavity.value(), "20240912/20240912.brep")
    grunk.reflect.invoke("grocc::export_to_step", cavity.value(), "20240912/20240912.stp")