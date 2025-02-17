import grunk
import numpy as np


def seat_cushion_recipe():

    w_armrest                    = grunk.Feature("w_armrest", "double", 0.05)
    
    w_sitting_surface            = grunk.Feature("w_sitting_surface", "double", 0.45)
    h_sitting_surface            = grunk.Feature("h_sitting_surface", "double", 0.4)
    h_cushion                    = grunk.Feature("h_cushion", "double", 0.12)
    l_cushion                    = grunk.Feature("l_cushion", "double", 0.45)
    seat_fillet_radius_leftright = grunk.Feature("seat_fillet_radius_leftright", "double", 0.01)
    seat_fillet_radius_frontback = grunk.expression("seat_fillet_radius_frontback", "h_cushion / 2.05", h_cushion)
    
    seat_y = grunk.expression("seat_y", "- w_armrest / 2", w_armrest)
    seat_z = grunk.expression("seat_z", "h_sitting_surface - h_cushion", h_sitting_surface, h_cushion)
    seat_pos = grunk.action("seat_pos", "grocc::gp_Pnt", 0., seat_y, seat_z).output()
    w_cushion = grunk.expression("w_cushion", "w_sitting_surface + w_armrest", w_sitting_surface, w_armrest)
    seat_box_s = grunk.action("seat_box_s", "grocc::BRepPrimAPI_MakeBox", seat_pos, l_cushion, w_cushion, h_cushion).output()
    seat_box = grunk.action("seat_box", "geo::Shape", seat_box_s).output()

    f1_i1 = grunk.Feature("f1_i1", "int", 1)
    f1_i3 = grunk.Feature("f1_i3", "int", 3)
    edges_front_s = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["edges_front_s"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "edges_front_s"]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge1"], [seat_box, f1_i1]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge2"], [seat_box, f1_i3]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_front_s", "edge1"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_front_s", "edge2"]),
        ],
        returns=["edges_front_s"]
    ).output()
    edges_front = grunk.action("edges_front", "geo::Shape", edges_front_s).output()
    f1 = grunk.action("f1", "geo::make_fillet", seat_box, edges_front, seat_fillet_radius_frontback).output()

    f2_i0 = grunk.Feature("", "int", 0)
    f2_i3 = grunk.Feature("", "int", 3)
    edges_back_s = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["edges_back_s"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "edges_back_s"]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge1"], [f1, f2_i0]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge2"], [f1, f2_i3]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_back_s", "edge1"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_back_s", "edge2"]),
        ],
        returns=["edges_back_s"]
    ).output()
    edges_back = grunk.action("edges_back", "geo::Shape", edges_back_s).output()
    seat_cushion = grunk.action("seat_cushion", "geo::make_fillet", f1, edges_back, seat_fillet_radius_leftright).output()
    return grunk.Recipe(seat_cushion, w_sitting_surface, h_sitting_surface, h_cushion, l_cushion, w_armrest)


def backrest_recipe():

    w_sitting_surface            = grunk.Feature("w_sitting_surface", "double", 0.45)
    h_sitting_surface            = grunk.Feature("h_sitting_surface", "double", 0.4)
    h_cushion                    = grunk.Feature("h_cushion", "double", 0.12)
    l_cushion                    = grunk.Feature("l_cushion", "double", 0.45)

    dx_back                      = grunk.Feature("dx_back", "double", 0.05)
    h_back                       = grunk.Feature("h_back", "double", 0.82)
    backrest_radius_leftright    = grunk.Feature("backrest_radius_leftright", "double", 0.1)
    backrest_radius_top          = grunk.expression("backrest_radius_top", "0.9 * dx_back", dx_back)

    t_headrest                   = grunk.Feature("t_headrest", "double", 0.02)
    border_headrest              = grunk.Feature("border_headrest", "double", 0.02)
    h_headrest                   = grunk.Feature("h_headrest", "double", 0.2)
    headrest_radius              = grunk.Feature("headrest_radius", "double", 0.08)
    headrest_radius2             = grunk.Feature("headrest_radius2", "double", 0.015)

    phi_recline                  = grunk.Feature("phi_recline", "double", 10.0 / 180.0 * np.pi)

    seat_z = grunk.expression("seat_z", "h_sitting_surface - h_cushion", h_sitting_surface, h_cushion)
    backrest_posz = grunk.expression("backrest_posz", "h_sitting_surface - h_cushion", h_sitting_surface, h_cushion)
    backrest_pos = grunk.action("backrest_pos", "grocc::gp_Pnt", l_cushion, 0., seat_z).output()
    backrest_box_s = grunk.action("backrest_box_s", "grocc::BRepPrimAPI_MakeBox", backrest_pos, dx_back, w_sitting_surface, h_back).output()
    backrest_box = grunk.action("backrest_box", "geo::Shape", backrest_box_s).output()

    f3_i09 = grunk.Feature("", "int", 9)
    f3_i11 = grunk.Feature("", "int", 11)
    edges_top_s = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["edges_top_s"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "edges_top_s"]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge1"], [backrest_box_s, f3_i09]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge2"], [backrest_box_s, f3_i11]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_top_s", "edge1"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_top_s", "edge2"]),
        ],
        returns=["edges_top_s"]
    ).output()
    edges_top = grunk.action("edges_top", "geo::Shape", edges_top_s).output()
    f3 = grunk.action("f3", "geo::make_fillet", backrest_box, edges_top, backrest_radius_leftright).output()
    
    f4_i06 = grunk.Feature("", "int", 6)
    edge_back_s = grunk.action("edge_back_s", "geo::internal::GetEdge", f3, f4_i06).output()
    edge_back = grunk.action("edge_back", "geo::Shape", edge_back_s).output()
    backrest = grunk.action("backrest", "geo::make_fillet", f3, edge_back, backrest_radius_top).output()

    # define headrest
    headrest_posx = grunk.expression("headrest_posx", "l_cushion - t_headrest", l_cushion, t_headrest)
    headrest_posz = grunk.expression("backrest_posz", "h_sitting_surface + h_back - h_cushion - h_headrest - border_headrest", h_sitting_surface, h_back, h_cushion, h_headrest, border_headrest)
    headrest_width = grunk.expression("headrest_width", "w_sitting_surface - 2 * border_headrest", w_sitting_surface, border_headrest)
    headrest_pos = grunk.action("headrest_pos", "grocc::gp_Pnt", headrest_posx, border_headrest, headrest_posz).output()
    headrest_box_s = grunk.action("headrest_box_s", "grocc::BRepPrimAPI_MakeBox", headrest_pos, t_headrest, headrest_width, h_headrest).output()
    headrest_box = grunk.action("headrest_box", "geo::Shape", headrest_box_s).output()

    f5_i08 = grunk.Feature("", "int", 8)
    f5_i09 = grunk.Feature("", "int", 9)
    f5_i10 = grunk.Feature("", "int",10)
    f5_i11 = grunk.Feature("", "int",11)
    edges_side_s = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["edges_side_s"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "edges_side_s"]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge1"], [headrest_box, f5_i08]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge2"], [headrest_box, f5_i09]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge3"], [headrest_box, f5_i10]),
            grunk.ScriptStep("geo::internal::GetEdge", ["edge4"], [headrest_box, f5_i11]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_side_s", "edge1"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_side_s", "edge2"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_side_s", "edge3"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "edges_side_s", "edge4"])
        ],
        returns=["edges_side_s"]
    ).output()
    edges_side = grunk.action("edges_side", "geo::Shape", edges_side_s).output()
    f5 = grunk.action("f5", "geo::make_fillet", headrest_box, edges_side, headrest_radius).output()
    
    f6_i0 = grunk.Feature("", "int", 1)
    f5_i00 = grunk.Feature("", "int", 0)
    headrest_front_face_s = grunk.action("headrest_front_face_s", "geo::internal::GetFace", f5, f5_i00).output()
    headrest_front_face = grunk.action("headrest_front_face", "geo::Shape", headrest_front_face_s).output()    
    headrest = grunk.action("headrest", "geo::make_fillet", f5, headrest_front_face, headrest_radius2).output()

    back = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["back"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "back"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "back", backrest]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "back", headrest]),
        ],
        returns=["back"]
    ).output()

    # recline back
    rot_pnt = grunk.action("rot_pnt", "grocc::gp_Pnt", l_cushion, 0., h_sitting_surface).output()
    rot_dir = grunk.Feature("rot_dir", "grocc::gp_Dir", 0., 1., 0.)
    rot_ax = grunk.action("rot_ax", "grocc::gp_Ax1", rot_pnt, rot_dir).output()
    backrest_reclined = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Trsf",["t"],[]),
            grunk.ScriptStep("grocc::gp_Trsf::SetRotation", [], ["t", rot_ax, phi_recline]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["backrest_reclined"], [back, "t"])
        ],
        returns=["backrest_reclined"]

    ).output()
    return grunk.Recipe(backrest_reclined, dx_back, phi_recline, h_cushion, l_cushion, h_sitting_surface, w_sitting_surface)


def armrest_recipe():

    h_sitting_surface            = grunk.Feature("h_sitting_surface", "double", 0.4)
    l_cushion                    = grunk.Feature("l_cushion", "double", 0.45)
    dx_back                      = grunk.Feature("dx_back", "double", 0.05)
    phi_recline                  = grunk.Feature("phi_recline", "double", 10.0 / 180.0 * np.pi)

    w_armrest                    = grunk.Feature("w_armrest", "double", 0.05)
    h_armrest                    = grunk.Feature("h_armrest", "double", 0.2)
    armrest_radius               = grunk.Feature("armrest_radius", "double", 0.05)
    
    l_armrest = grunk.expression("l_armrest", "0.75*l_cushion", l_cushion)
    armrest_posx = grunk.expression("armrest_posx", "l_cushion - l_armrest + dx_back * cos(phi_recline) / 2", l_cushion, l_armrest, dx_back, phi_recline)
    armrest_posy = grunk.expression("armrest_posy", "- w_armrest", w_armrest)
    armrest_pos = grunk.action("armrest_pos", "grocc::gp_Pnt", armrest_posx, armrest_posy, h_sitting_surface).output()
    armrest_dx = grunk.expression("armrest_dx", "l_armrest + h_armrest * sin(phi_recline)", l_armrest, h_armrest, phi_recline)
    armrest_box_s = grunk.action("armrest_box_s", "grocc::BRepPrimAPI_MakeBox", armrest_pos, armrest_dx, w_armrest, h_armrest).output()
    armrest_box = grunk.action("armrest_box", "geo::Shape", armrest_box_s).output()

    armrest_edge_s = grunk.action("armrest_edge_s", "geo::internal::GetEdge", armrest_box, 1).output()
    armrest_edge = grunk.action("armrest_edge", "geo::Shape", armrest_edge_s).output()
    armrest_nobop = grunk.action("armrest_nobop", "geo::make_fillet", armrest_box, armrest_edge, armrest_radius).output()

    pnt_x = grunk.expression("pnt_x", "l_cushion + dx_back * cos(phi_recline) / 2.0", l_cushion, dx_back, phi_recline)
    pnt_y = grunk.Feature("", "double", 0.)
    dir_x = grunk.expression("dir_x", "cos(phi_recline)", phi_recline)
    dir_y = grunk.Feature("", "double", 0.)
    dir_z = grunk.expression("dir_z", "-sin(phi_recline)", phi_recline)
    uvmin = grunk.Feature("", "double", 0.)
    uvmax = grunk.Feature("", "double", 1.)
    armrest = grunk.script(
        [
            grunk.ScriptStep("grocc::gp_Pnt", ["pln_pnt"], [pnt_x, pnt_y, h_sitting_surface]),
            grunk.ScriptStep("grocc::gp_Dir", ["pln_dir"], [dir_x, dir_y, dir_z]),
            grunk.ScriptStep("grocc::gp_Pln", ["pln"], ["pln_pnt", "pln_dir"]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_MakeFace", ["face_maker"], ["pln", uvmin, uvmax, uvmin, uvmax]),
            grunk.ScriptStep("grocc::BRepBuilderAPI_MakeFace::Face", ["face"], ["face_maker"]),
            grunk.ScriptStep("grocc::gp_Pnt::Translated", ["pnt"], ["pln_pnt", "pln_dir"]),
            grunk.ScriptStep("grocc::BRepPrimAPI_MakeHalfSpace", ["halfspace"], ["face", "pnt"]),
            grunk.ScriptStep("grocc::BRepAlgoAPI_Cut", ["armrest"], [armrest_nobop, "halfspace"])
        ],
        returns = ["armrest"]
    ).output()
    return grunk.Recipe(armrest, h_sitting_surface, dx_back, l_cushion, phi_recline, w_armrest)


def single_seat_recipe():
    
    w_sitting_surface            = grunk.Feature("w_sitting_surface", "double", 0.45)
    w_armrest                    = grunk.Feature("w_armrest", "double", 0.05)

    recipe = grunk.Recipe(w_armrest, w_sitting_surface)
    recipe.insert_recipe("seat_cushion", seat_cushion_recipe())
    recipe.insert_recipe("backrest", backrest_recipe())

    # evaluate seat_cushion recipe
    recipe.recipe(
        "seat_cushion", 
        {
            # outputs to be passed from seat_cushion recipe to seat_recipe
            "h_sitting_surface" : "h_sitting_surface",
            "h_cushion" : "h_cushion",
            "l_cushion" : "l_cushion",
            "cushion" : "seat_cushion",
        }, 
        {   
            #inputs to be passed from seat_recipe to seat_cushion recipe
            "w_armrest" : w_armrest, 
            "w_sitting_surface": w_sitting_surface
        }
    )
    
    # evaluate backrest recipe
    recipe.recipe(
        "backrest", 
        {
            # outputs to be passed from backrest recipe to seat_recipe
            "back": "backrest_reclined",
            "dx_back" : "dx_back",
            "phi_recline" : "phi_recline"
        }, 
        {
            #inputs to be passed from seat_recipe to backrest recipe
            "w_sitting_surface" : w_sitting_surface,
            "h_sitting_surface" : recipe["h_sitting_surface"],
            "h_cushion" : recipe["h_cushion"],
            "l_cushion" : recipe["l_cushion"]
        }
    )

    # make everything a compound
    single_seat = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["single_seat"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "single_seat"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "single_seat", recipe["cushion"]]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "single_seat", recipe["back"]]),
        ],
        returns = ["single_seat"]
    ).output()
    recipe.insert_feature(single_seat)

    return recipe


def support_recipe():

    h_sitting_surface            = grunk.Feature("h_sitting_surface", "double", 0.4)
    h_cushion                    = grunk.Feature("h_cushion", "double", 0.12)
    l_cushion                    = grunk.Feature("l_cushion", "double", 0.45)

    w_support   = grunk.Feature("w_support", "double", 0.03)
    dx_support  = grunk.expression("dx_support", "0.5*l_cushion", l_cushion)

    support_x = grunk.expression("support_x", "(l_cushion - dx_support) / 2", l_cushion, dx_support)
    support_pos = grunk.action("support_pos", "grocc::gp_Pnt", support_x, 0., 0.).output()
    dz_support = grunk.expression("dz_support", "h_sitting_surface - h_cushion", h_sitting_surface, h_cushion)
    support = grunk.action("support", "grocc::BRepPrimAPI_MakeBox", support_pos, dx_support, w_support, dz_support).output()
    return grunk.Recipe(support, w_support, l_cushion, h_cushion, h_sitting_surface)

def seat_row_recipe():

    n_seats                      = grunk.Feature("n_seats", "int", 4)
    n_supports                   = grunk.Feature("n_supports", "int", 3)

    w_sitting_surface            = grunk.Feature("w_sitting_surface", "double", 0.45)
    w_armrest                    = grunk.Feature("w_armrest", "double", 0.05)

    recipe = grunk.Recipe()
    recipe.insert_recipe("seat", single_seat_recipe())
    recipe.insert_recipe("support", support_recipe())
    recipe.insert_recipe("armrest", armrest_recipe())


    # evaluate seat recipe
    recipe.recipe(
        "seat",
        {
            # outputs to be passed from armrest recipe to seat_recipe
            "seat": "single_seat",
            "h_sitting_surface" : "h_sitting_surface",
            "h_cushion" : "h_cushion",
            "l_cushion" : "l_cushion",
            "dx_back" : "dx_back",
            "phi_recline" : "phi_recline"
        },
        {
            #inputs to be passed from seat_recipe to armrest recipe
            "w_sitting_surface" : w_sitting_surface,
            "w_armrest" : w_armrest
        }
    )

    dy_seat = grunk.expression("dy_seat", "w_sitting_surface + w_armrest", w_sitting_surface, w_armrest)
    seat_repeat_dir = grunk.action("seat_repeat_dir", "grocc::gp_Vec", 0., dy_seat, 0.).output()
    
    seat_dy_start = grunk.expression("seat_dy_start", "-0.5*dy_seat * n_supports + w_armrest/2", dy_seat, n_supports, w_armrest)
    seat_repeat_start = grunk.action("seat_repeat_start", "grocc::gp_Vec", 0., seat_dy_start, 0.).output()
    
    seat_start = grunk.action("seat_start", "geo::translate", recipe["seat"], seat_repeat_start).output()
    seats = grunk.action("seats", "geo::repeat_shape", seat_start, seat_repeat_dir, n_seats).output()

    # evaluate armrest recipe
    recipe.recipe(
        "armrest", 
        {
            # outputs to be passed from armrest recipe to seat_recipe
            "armrest": "armrest"
        }, 
        {
            #inputs to be passed from seat_recipe to armrest recipe
            "w_armrest" : w_armrest,
            "h_sitting_surface" : recipe["h_sitting_surface"],
            "l_cushion" : recipe["l_cushion"],
            "dx_back" : recipe["dx_back"],
            "phi_recline" : recipe["phi_recline"]
        }
    )

    n_armrests = grunk.expression("n_armrests", "n_seats+1", n_seats)
    armrest_start = grunk.action("armrest_start", "geo::translate", recipe["armrest"], seat_repeat_start).output()
    armrests = grunk.action("armrests", "geo::repeat_shape", armrest_start, seat_repeat_dir, n_armrests).output()

    # evaluate support
    recipe.recipe(
        "support",
        {
            "support": "support",
            "w_support": "w_support"
        },
        {
            "h_sitting_surface" : recipe["h_sitting_surface"],
            "h_cushion" : recipe["h_cushion"],
            "l_cushion" : recipe["l_cushion"]
        }
    )

    dy_support = grunk.expression("dy_support", "(n_seats+1)*dy_seat/(n_supports+2)", n_seats, dy_seat, n_supports)
    support_repeat_dir = grunk.action("support_repeat_dir", "grocc::gp_Vec", 0., dy_support, 0.).output()
    
    support_dy_start = grunk.expression(
        "support_dy_start", 
        "seat_dy_start + dy_support - w_support/2 - w_armrest/2", 
        seat_dy_start, dy_support, recipe["w_support"], w_armrest
    )
    support_repeat_start = grunk.action("support_repeat_start", "grocc::gp_Vec", 0., support_dy_start, 0.).output()

    support_start = grunk.action("support_start", "geo::translate", recipe["support"], support_repeat_start).output()
    supports = grunk.action("supports", "geo::repeat_shape", support_start, support_repeat_dir, n_supports).output()

    row = grunk.script(
        [
            grunk.ScriptStep("grocc::TopoDS_Compound", ["row"], []),
            grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
            grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "row"]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "row", seats]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "row", supports]),
            grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "row", armrests])
        ],
        returns = ["row"]
    ).output()

    for f in [row, n_seats, w_sitting_surface, w_armrest, n_supports, recipe["w_support"]]:
        recipe.insert_feature(f)

    return recipe

if __name__ == '__main__':

    grunk.get_plugin_registry().load_env("seat_example")

    seat_model = seat_row_recipe()
    grunk.write("seat_model.grr.yml", seat_model)

    for n_seats in range(2,5):
        seat_model["n_seats"].set_value(n_seats)
        for width_cm in range(35, 56, 10):
            seat_model["w_sitting_surface"].set_value(width_cm/100)

            filename = f"seat_row_n_{n_seats}_w_{width_cm}.brep"
            grunk.reflect.invoke("grocc::BRepTools::Write", seat_model["row"].value(), filename)
