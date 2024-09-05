import grunk

grunk.init()
grunk.get_plugin_registry().load_env("tigl")
# grunk.help("tigl::generated::CPACSComponentSegment")
recipe = grunk.read("CPACS_30_D150_wingbox.grr.yml")

spar_le = recipe["spar_le_shape"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", spar_le, "spar_le.brep")

spar_te = recipe["spar_te_shape"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", spar_te, "spar_te.brep")

rib_root = recipe["rib_root_shape"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", rib_root, "rib_root.brep")

rib_tip = recipe["rib_tip_shape"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", rib_tip, "rib_tip.brep")

wing_shape = recipe["wing_shape"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", wing_shape, "wing_shape.brep")

split4 = recipe["split4"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", split4, "split4.brep")


recipe_fuel = grunk.read("fuelsystem.grr.yml")
fuel_system = recipe_fuel["compound"].value()
grunk.reflect.invoke("grocc::BRepTools::Write", fuel_system, "fuel_system.brep")