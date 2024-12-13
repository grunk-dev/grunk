import grunk
import math

def cavity_sketch():
    # build cavity_sketch
    s = grunk.Feature("s", "double", 1)
    R2 = grunk.Feature("R2", "double", 100)
    R1 = grunk.Feature("R1", "double", 50)
    # lkginw = grunk.Feature("lkginw", "double", 0.1)
    # lkginh = grunk.Feature("lkginh", "double", 0.1)
    # lkgoutw = grunk.Feature("lkgoutw", "double", 0.1)
    # lkgouth = grunk.Feature("lkgouth", "double", 0.1)

    cavity_height = grunk.expression("cavity_height", "R2 - R1", R1, R2)

    # initial model without leakage
    points = [
        grunk.action("p0", "grocc::gp_Pnt", 0., 0., 0.).output(),
        grunk.action("p1", "grocc::gp_Pnt", s, 0., 0.).output(),
        grunk.action("p2", "grocc::gp_Pnt", s, cavity_height, 0.).output(),
        grunk.action("p3", "grocc::gp_Pnt", 0., cavity_height, 0.).output(),
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
    #return grunk.Recipe(face, s, R1, R2, lkginw, lkginh, lkgoutw, lkgouth)
    return grunk.Recipe(face, s, R1, R2)

if __name__ == '__main__':

    grunk.init()
    grunk.get_plugin_registry().load_env("cad")

    # build cavity recipe
    s_stator = grunk.Feature("s_stator", "double", 5)
    s_rotor = grunk.Feature("s_rotor", "double", 5)
    R1 = grunk.Feature("R1", "double", 203.2)
    R2 = grunk.Feature("R2", "double", 250)
    # lkginw = grunk.Feature("lkginw", "double", 1.)
    # lkginh = grunk.Feature("lkginh", "double", 1.)
    # lkgoutw = grunk.Feature("lkgoutw", "double", 1.)
    # lkgouth = grunk.Feature("lkgouth", "double", 1.)
    
    cavity_recipe = grunk.Recipe()
    # stator cavity part
    cavity_recipe.insert_recipe("cavity_sketch", cavity_sketch())
    cavity_recipe.recipe(
        "cavity_sketch",
        {
            #outputs to be passed from cavity_sketch to cavity_recipe
            "cavity_face_stator" : "face"
        },
        {
            #inputs to be passed from cavity_recipe to cavity_sketch
            "R1" : R1,
            "R2" : R2,
            "s" : s_stator,
        #   "lkginw" : lkginw,
        #   "lkginh" : lkginh,
        #   "lkgoutw" : lkgoutw,
        #   "lkgouth" : lkgouth
        }
    )

    o = grunk.action("o", "grocc::gp_Pnt", 0., 0., 0.).output()
    d = grunk.action("d", "grocc::gp_Dir", -1., 0., 0.).output()
    ax = grunk.action("ax", "grocc::gp_Ax1", o, d).output()
    alpha = grunk.Feature("alpha", "double", 20) # Angle of nozzle
    alpha_rad = grunk.expression("alpha_rad", f"{math.pi}*alpha/180.", alpha)
    N_N = grunk.Feature("N_N", "double", 12)
    angle_rad_stator = grunk.expression("angle_rad_stator", f"{math.pi}*360/N_N/180.", N_N)
    angle_diff_stator = grunk.expression("angle_diff_stator", "acos(R1/sqrt(((s_stator+s_rotor)/tan(alpha_rad))^2+R1^2))", R1, s_stator, s_rotor, alpha_rad)
    angle_sector_stator_rad = grunk.expression("angle_sector_stator_rad", "(-angle_rad_stator/2)+angle_diff_stator", angle_rad_stator, angle_diff_stator)

    translation_stator = grunk.action("translation_stator", "grocc::gp_Vec", 0., R1, 0.).output()
    translated_face_stator = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Trsf", ["trsf"], []),
            grunk.ScriptStep("grocc::gp_Trsf::SetTranslation", [""], ["trsf", translation_stator]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["translated_face_stator"], [cavity_recipe["cavity_face_stator"], "trsf"])
        ],
        returns = ["translated_face_stator"]
    ).output()

    rotated_face_stator = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Trsf", ["trsf2"], []),
            grunk.ScriptStep("grocc::gp_Trsf::SetRotation", [""], ["trsf2", ax, angle_sector_stator_rad]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["rotated_face_stator"], [translated_face_stator, "trsf2"])
        ],
        returns = ["rotated_face_stator"]
    ).output()

    revol_stator = grunk.action("revol_stator", "grocc::BRepPrimAPI_MakeRevol", rotated_face_stator, ax, angle_rad_stator).output()

    # rotor cavity part
    cavity_recipe.recipe(
        "cavity_sketch",
        {
            #outputs to be passed from cavity_sketch to cavity_recipe
            "cavity_face_rotor" : "face"
        },
        {
            #inputs to be passed from cavity_recipe to cavity_sketch
            "R1" : R1,
            "R2" : R2,
            "s" : s_rotor,
        #   "lkginw" : lkginw,
        #   "lkginh" : lkginh,
        #   "lkgoutw" : lkgoutw,
        #   "lkgouth" : lkgouth
        }
    )

    N_R = grunk.Feature("N_R", "double", 12)
    angle_rad_rotor = grunk.expression("angle_rad_rotor", f"{math.pi}*360/N_R/180.", N_R)
    angle_sector_rotor_rad = grunk.expression("angle_sector_rotor_rad", "-angle_rad_rotor/2", angle_rad_rotor)

    translation_rotor = grunk.action("translation_rotor", "grocc::gp_Vec", s_stator, R1, 0.).output()
    translated_face_rotor = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Trsf", ["trsf"], []),
            grunk.ScriptStep("grocc::gp_Trsf::SetTranslation", [""], ["trsf", translation_rotor]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["translated_face_rotor"], [cavity_recipe["cavity_face_rotor"], "trsf"])
        ],
        returns = ["translated_face_rotor"]
    ).output()

    rotated_face_rotor = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Trsf", ["trsf2"], []),
            grunk.ScriptStep("grocc::gp_Trsf::SetRotation", [""], ["trsf2", ax, angle_sector_rotor_rad]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["rotated_face_rotor"], [translated_face_rotor, "trsf2"])
        ],
        returns = ["rotated_face_rotor"]
    ).output()

    revol_rotor = grunk.action("revol_rotor", "grocc::BRepPrimAPI_MakeRevol", rotated_face_rotor, ax, angle_rad_rotor).output()

    # Fuse of stator and rotor cavity part
    #revol = grunk.action("revol", "grocc::BRepAlgoAPI_Fuse", revol_stator, revol_rotor).output()

    revol = grunk.script(
            [
                grunk.ScriptStep("grocc::TopoDS_CompSolid", ["assembly"], []),
                grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
                grunk.ScriptStep("grocc::BRep_Builder::MakeCompSolid", [], ["aBuilder", "assembly"]),
                grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "assembly", revol_stator]),
                grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "assembly", revol_rotor]),
            ],
            returns=["assembly"]
        ).output()

    # build recipe for cylinder and add to cavity recipe
    cyl_origin = grunk.Feature("origin", "grocc::gp_Pnt", 0., 0., 0.)
    cyl_dir = grunk.Feature("direction", "grocc::gp_Dir", 1., 0., 0.)
    cyl_dir2 = grunk.Feature("direction2", "grocc::gp_Dir", 0., 1., 0.)
    d = grunk.Feature("d", "double", 1.)
    cyl_radius = grunk.expression("cyl_radius", "d/2", d)
    l = grunk.Feature("l", "double", 1.)
    ax2 = grunk.action("ax2", "grocc::gp_Ax2", cyl_origin, cyl_dir, cyl_dir2).output()
    cyl = grunk.action("cylinder", "grocc::BRepPrimAPI_MakeCylinder", ax2, cyl_radius, l).output()
    cylinder_recipe = grunk.Recipe(cyl, l, cyl_radius, cyl_origin, cyl_dir)
    cavity_recipe.insert_recipe("cylinder", cylinder_recipe)

    # add first cylinder to cavity recipe and fuse with cavity (receiver hole)
    d_R = grunk.Feature("d_R", "double", 10)
    l_R = grunk.Feature("l_R", "double", 150)
    R_R = grunk.Feature("R_R", "double", 220)
    cyl_origin_x = grunk.expression("cyl1_pos_x", "0.75*(s_stator+s_rotor)", s_stator, s_rotor)
    l_R_add = grunk.expression("l_R_add", "l_R+0.25*(s_stator+s_rotor)", l_R, s_stator, s_rotor)
    cyl_origin_y = R_R
    cyl_origin_z = 0
    start = grunk.action("cyl1_origin", "grocc::gp_Pnt", cyl_origin_x, cyl_origin_y, cyl_origin_z).output()
    cavity_recipe.recipe(
        "cylinder",
        {
            "cylinder1" : "cylinder",
        },
        {
            "d" : d_R,
            "l" : l_R_add,
            "origin" : start
        }
    )
    revol_cyl1 = grunk.action("cavity_cyl1", "grocc::BRepAlgoAPI_Fuse", revol, cavity_recipe["cylinder1"]).output()

    # add second cylinder to cavity recipe and fuse with cavity (nozzle)
    d_N = grunk.Feature("d_N", "double", 8)
    l_N = grunk.Feature("l_N", "double", 150)
    # added length for shift
    l_N_add = grunk.expression("l_N_add", "l_N+((s_stator+s_rotor)/sin(alpha_rad))", l_N, s_stator, s_rotor, alpha_rad)
    R_N = grunk.Feature("R_N", "double", 220)
    # cyl_origin_x = 0
    # cyl_origin_y = grunk.expression("cyl2_pos_y", "R_N*cos(angle_rad_stator/2)", R_N, angle_rad_stator)
    cyl_origin_x = grunk.expression("cyl2_pos_x", "s_stator+s_rotor", s_stator, s_rotor)
    # cyl_origin_z = grunk.expression("cyl2_pos_z", "-R_N*sin(angle_rad_stator/2)", R_N, angle_rad_stator)
    start = grunk.action("cyl2_origin", "grocc::gp_Pnt", cyl_origin_x, cyl_origin_y, cyl_origin_z).output()
    # same start point as first cylinder
    # alpha definition see above
    dir_x = grunk.expression("dir_x", "-sin(alpha_rad)", alpha_rad)
    dir_z = grunk.expression("dir_z", "-cos(alpha_rad)", alpha_rad)
    dir = grunk.action("cyl2_dir", "grocc::gp_Dir", dir_x, 0., dir_z).output()
    cavity_recipe.recipe(
        "cylinder",
        {
            "cylinder2" : "cylinder",
        },
        {
            "d" : d_N,
            "l" : l_N_add,
            "origin" : start,
            "direction": dir
        }
    )
    # cylinder2 = grunk.script(
    #     [
    #         grunk.ScriptStep("grocc::gp_Vec", ["t1"], [dir]),
    #         grunk.ScriptStep("grocc::gp_Vec::Multiplied", ["transl"], ["t1", cyl2_shift]),
    #         grunk.ScriptStep("grocc::gp_Trsf", ["trsf"], []),
    #         grunk.ScriptStep("grocc::gp_Trsf::SetTranslation", [""], ["trsf", "transl"]),
    #         grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["cylinder2"], [cavity_recipe["cylinder2_tmp"], "trsf"])
    #     ],
    #     returns = ["cylinder2"]
    # ).output()

    # cut away at cavity wall
    xcut_Nout = grunk.expression("xcut_Nout", "(s_stator+s_rotor)*0", s_stator, s_rotor) # falls es in der Verbindung zwischen PSN und Statorkavitätswand Probleme gibt, hier eine Überscheidung (d.h. Faktor > 0) vorsehen
    xcut_temp = grunk.expression("xcut_temp1", "-xcut_Nin*2", xcut_Nout)
    pnt_Nout = grunk.action("pnt_Nout", "grocc::gp_Pnt", xcut_Nout, 0., 0.).output()
    dir = grunk.action("x_axis", "grocc::gp_Dir", 1., 0., 0.).output()
    pln_Nout = grunk.action("pln_Nout", "grocc::gp_Pln", pnt_Nout, dir).output()
    f = grunk.action("halfspace_face1", "grocc::BRepBuilderAPI_MakeFace", pln_Nout).output()
    pnt2 = grunk.action("halfpace_pnt1", "grocc::gp_Pnt", xcut_temp, 0., 0.).output()
    halfspace = grunk.action("halfspace1", "grocc::BRepPrimAPI_MakeHalfSpace", f, pnt2).output()
    cylinder2_cut1 = grunk.action("clyinder2_cut1", "grocc::BRepAlgoAPI_Cut", cavity_recipe["cylinder2"], halfspace).output()

    # cut away at nozzle inlet; for inlet flow in nozzle axis direction use a high value for l_ax_N (> l_N), otherwise l_ax_N is the axial length of nozzle after cut
    l_ax_N = grunk.Feature("l_ax_N", "double", 200)
    xcut_Nin = grunk.expression("xcut_Nin", "l_ax_N*(-1)", l_ax_N)
    xcut_temp = grunk.expression("xcut_temp2", "xcut_Nin*2", xcut_Nin)
    pnt_Nin = grunk.action("pnt_Nin", "grocc::gp_Pnt", xcut_Nin, 0., 0.).output()
    pln_Nin = grunk.action("pln_Nin", "grocc::gp_Pln", pnt_Nin, dir).output()
    f = grunk.action("halfspace_face2", "grocc::BRepBuilderAPI_MakeFace", pln_Nin).output()
    pnt2 = grunk.action("halfpace_pnt2", "grocc::gp_Pnt", xcut_temp, 0., 0.).output()
    halfspace = grunk.action("halfspace2", "grocc::BRepPrimAPI_MakeHalfSpace", f, pnt2).output()
    cylinder2_cut2 = grunk.action("clyinder2_cut2", "grocc::BRepAlgoAPI_Cut", cylinder2_cut1, halfspace).output()
    
    cavity = grunk.action("cavity", "grocc::BRepAlgoAPI_Fuse", revol_cyl1, cylinder2_cut2).output()
    cavity_recipe.insert_feature(cavity)

    grunk.write("20241017/20241017.grr.yml", cavity_recipe)
