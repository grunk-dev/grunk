import grunk
import math

def cavity_sketch():
    # build cavity_sketch
    cavity_w = grunk.Feature("cavity_w", "double", 1)
    cavity_h = grunk.Feature("cavity_h", "double", 1)
    dw = grunk.Feature("dw", "double", 0.1)
    dh = grunk.Feature("dh", "double", 0.1)

    width = grunk.expression("width", "cavity_w + dw", cavity_w, dw)
    h1 = grunk.expression("h1", "cavity_h - dh", cavity_h, dh)

    points = [
        grunk.action("p0", "grocc::gp_Pnt", 0., 0., 0.).output(),
        grunk.action("p1", "grocc::gp_Pnt", width, 0., 0.).output(),
        grunk.action("p2", "grocc::gp_Pnt", width, cavity_h, 0.).output(),
        grunk.action("p3", "grocc::gp_Pnt", 0., cavity_h, 0.).output(),
        grunk.action("p4", "grocc::gp_Pnt", 0., h1, 0.).output(),
        grunk.action("p5", "grocc::gp_Pnt", dw, h1, 0.).output(),
        grunk.action("p6", "grocc::gp_Pnt", dw, dh, 0.).output(),
        grunk.action("p7", "grocc::gp_Pnt", 0., dh, 0.).output(),
    ]
        
    polygon = grunk.script(
        [
            grunk.ScriptStep("grocc::BRepBuilderAPI_MakePolygon", ["polygon"], []),
            *[grunk.ScriptStep("grocc::BRepBuilderAPI_MakePolygon::Add", [], ["polygon", p]) for p in points],
            grunk.ScriptStep("grocc::BRepBuilderAPI_MakePolygon::Close", [], ["polygon"]),
        ],
        returns = ["polygon"]
    ).output()

    face = grunk.action("face", "grocc::BRepBuilderAPI_MakeFace", polygon).output()
    return grunk.Recipe(face, cavity_w, cavity_h, dw, dh)


if __name__ == '__main__':

    grunk.init()
    grunk.get_plugin_registry().load_env("cad")

    # build cavity recipe
    cavity_w = grunk.Feature("cavity_w", "double", 20.8)
    cavity_h = grunk.Feature("cavity_h", "double", 50)
    dw = grunk.Feature("dw", "double", 1.)
    dh = grunk.Feature("dh", "double", 1.)

    cavity_recipe = grunk.Recipe()
    cavity_recipe.insert_recipe("cavity_sketch", cavity_sketch())
    cavity_recipe.recipe(
        "cavity_sketch",
        {
            #outputs to be passed from cavity_sketch to cavity_recipe
            "cavity_face" : "face"
        },
        {
            #inputs to be passed from cavity_recipe to cavity_sketch
            "cavity_h" : cavity_h,
            "cavity_w" : cavity_w,
            "dw" : dw, 
            "dh" : dh
        }
    )

    radius = grunk.Feature("radius", "double", 150)
    translation = grunk.action("translation", "grocc::gp_Vec", 0., radius, 0.).output()
    transformed_face = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Trsf", ["trsf"], []),
            grunk.ScriptStep("grocc::gp_Trsf::SetTranslation", [""], ["trsf", translation]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["transformed_face"], [cavity_recipe["cavity_face"], "trsf"])
        ],
        returns = ["transformed_face"]
    ).output()

    o = grunk.action("o", "grocc::gp_Pnt", 0., 0., 0.).output()
    d = grunk.action("d", "grocc::gp_Dir", -1., 0., 0.).output()
    ax = grunk.action("ax", "grocc::gp_Ax1", o, d).output()
    angle_deg = grunk.Feature("angle_deg", "double", 30)
    angle_rad = grunk.expression("angle_rad", f"{math.pi}*angle_deg/180.", angle_deg)
    revol = grunk.action("revol", "grocc::BRepPrimAPI_MakeRevol", transformed_face, ax, angle_rad).output()
    cavity_recipe.insert_feature(revol)

    # build recipe for cylinder and add to cavity recipe
    cyl_origin = grunk.Feature("origin", "grocc::gp_Pnt", 0., 0., 0.)
    cyl_dir = grunk.Feature("direction", "grocc::gp_Dir", 1., 0., 0.)
    cyl_dir2 = grunk.Feature("direction2", "grocc::gp_Dir", 0., 1., 0.)
    cyl_radius = grunk.Feature("radius", "double", 1.)
    cyl_height = grunk.Feature("height", "double", 1.)
    ax2 = grunk.action("ax2", "grocc::gp_Ax2", cyl_origin, cyl_dir, cyl_dir2).output()
    cyl = grunk.action("cylinder", "grocc::BRepPrimAPI_MakeCylinder", ax2, cyl_radius, cyl_height).output()
    cylinder_recipe = grunk.Recipe(cyl, cyl_height, cyl_radius, cyl_origin, cyl_dir)
    cavity_recipe.insert_recipe("cylinder", cylinder_recipe)

    # add first cylinder to cavity recipe and fuse with cavity
    cyl_r = grunk.Feature("cyl1_radius", "double", 1.5)
    cyl_h = grunk.Feature("cyl1_height", "double", 20)
    cyl_rel_h = grunk.Feature("cyl1_dh", "double", 0.33)
    rpos = grunk.expression("cyl1_pos_r", "radius + cyl1_dh*cavity_h", radius, cyl_rel_h, cavity_h)
    cyl_origin_x = grunk.expression("cyl1_pos_x", "0.75*cavity_w", cavity_w)
    cyl_origin_y = grunk.expression("cyl1_pos_y", "cyl1_pos_r*cos(angle_rad/2.)", rpos, angle_rad)
    cyl_origin_z = grunk.expression("cyl1_pos_z", "-cyl1_pos_r*sin(angle_rad/2.)", rpos, angle_rad)
    start = grunk.action("cyl1_origin", "grocc::gp_Pnt", cyl_origin_x, cyl_origin_y, cyl_origin_z).output()
    cavity_recipe.recipe(
        "cylinder",
        {
            "cylinder1" : "cylinder",
        },
        {
            "radius" : cyl_r,
            "height" : cyl_h,
            "origin" : start
        }
    )
    revol_cyl1 = grunk.action("cavity_cyl1", "grocc::BRepAlgoAPI_Fuse", revol, cavity_recipe["cylinder1"]).output()

    # add second cylinder to cavity recipe and fuse with cavity
    cyl_r = grunk.Feature("cyl2_radius", "double", 3)
    cyl_h = grunk.Feature("cyl2_height", "double", 50)
    cyl_rel_h = grunk.Feature("cyl2_dh", "double", 0.5)
    rpos = grunk.expression("cyl2_pos_r", "radius + cyl2_dh*cavity_h", radius, cyl_rel_h, cavity_h)
    cyl_origin_x = grunk.expression("cyl2_pos_x", "dw", dw)
    cyl_origin_y = grunk.expression("cyl2_pos_y", "cyl2_pos_r*cos(angle_rad/2.)", rpos, angle_rad)
    cyl_origin_z = grunk.expression("cyl2_pos_z", "-cyl2_pos_r*sin(angle_rad/2.)", rpos, angle_rad)
    start = grunk.action("cyl2_origin", "grocc::gp_Pnt", cyl_origin_x, cyl_origin_y, cyl_origin_z).output()
    dir = grunk.Feature("cyl2_dir", "grocc::gp_Dir", -2., 1., -2.)
    cavity_recipe.recipe(
        "cylinder",
        {
            "cylinder2_tmp" : "cylinder",
        },
        {
            "radius" : cyl_r,
            "height" : cyl_h,
            "origin" : start,
            "direction": dir
        }
    )
    cyl2_shift = grunk.Feature("cyl2_shift", "double", -10)
    cylinder2 = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Vec", ["t1"], [dir]),
            grunk.ScriptStep("grocc::gp_Vec::Multiplied", ["transl"], ["t1", cyl2_shift]),
            grunk.ScriptStep("grocc::gp_Trsf", ["trsf"], []),
            grunk.ScriptStep("grocc::gp_Trsf::SetTranslation", [""], ["trsf", "transl"]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["cylinder2"], [cavity_recipe["cylinder2_tmp"], "trsf"])
        ],
        returns = ["cylinder2"]
    ).output()

    # cut away cyl2 at xmin
    xmin = grunk.Feature("x_min", "double", -20)
    xmin2 = grunk.expression("x_min2", "x_min*2", xmin)
    pnt = grunk.action("x_min_pnt", "grocc::gp_Pnt", xmin, 0., 0.).output()
    dir = grunk.action("x_axis", "grocc::gp_Dir", 1., 0., 0.).output()
    pln = grunk.action("halfspace_pln", "grocc::gp_Pln", pnt, dir).output()
    f = grunk.action("halfspace_face", "grocc::BRepBuilderAPI_MakeFace", pln).output()
    pnt2 = grunk.action("halfpace_pnt", "grocc::gp_Pnt", xmin2, 0., 0.).output()
    halfspace = grunk.action("halfspace", "grocc::BRepPrimAPI_MakeHalfSpace", f, pnt2).output()
    cylinder2_cut = grunk.action("clyinder2_cut", "grocc::BRepAlgoAPI_Cut", cylinder2, halfspace).output()
    cavity = grunk.action("cavity", "grocc::BRepAlgoAPI_Fuse", revol_cyl1, cylinder2_cut).output()
    cavity_recipe.insert_feature(cavity)

    grunk.write("cavity_model.grr.yml", cavity_recipe)
