import grunk

if __name__ == '__main__':

    grunk.init()
    grunk.get_plugin_registry().load_env("cad")
    
    w = grunk.Feature("w", "double", 1)
    h = grunk.Feature("h", "double", 2)
    d = grunk.Feature("d", "double", 3)
    
    box = grunk.action("box", "grocc::BRepPrimAPI_MakeBox", w, h, d).output()
    
    grunk.write("box.grr.yml", box)
    