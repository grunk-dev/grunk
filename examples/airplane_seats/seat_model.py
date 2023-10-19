import grunk
import numpy as np

grunk.load("grocc", "0.1.1", install_missing=True)
grunk.load("geo", "0.2.0", install_missing=True)

recipe = grunk.Recipe()

# define input parameters
n_seats                      = grunk.Feature("n_seats", "int", 4)
n_supports                   = grunk.Feature("n_supports", "int", 3)
w_sitting_surface            = grunk.Feature("w_sitting_surface", "double", 0.45)


# recipe = grunk.Recipe(n_seats, n_supports, w_sitting_surface)

# define "anonymous" input parameters
w_armrest                    = grunk.Feature("w_armrest", "double", 0.05)
h_back                       = grunk.Feature("h_back", "double", 0.82)
phi_recline                  = grunk.Feature("phi_recline", "double", 10.0 / 180.0 * np.pi)
h_sitting_surface            = grunk.Feature("h_sitting_surface", "double", 0.4)
dz_cushion                   = grunk.Feature("dz_cushion", "double", 0.12)
l_cushion                    = grunk.Feature("l_cushion", "double", 0.45)
l_armrest                    = grunk.expression("l_armrest", "0.75*l_cushion", l_cushion)
h_armrest                    = grunk.Feature("h_armrest", "double", 0.2)
dx_back                      = grunk.Feature("dx_back", "double", 0.05)
dx_support                   = grunk.expression("dx_support", "0.5*l_cushion", l_cushion)
w_support                    = grunk.Feature("w_support", "double", 0.03)
t_headrest                   = grunk.Feature("t_headrest", "double", 0.02)
border_headrest              = grunk.Feature("border_headrest", "double", 0.02)
h_headrest                   = grunk.Feature("h_headrest", "double", 0.2)

# define seat cushion
seat_x = grunk.Feature("seat_x", "double", 0.)
seat_y = grunk.expression("seat_y", "-w_armrest / 2", w_armrest)
seat_z = grunk.expression("seat_z", "h_sitting_surface - dz_cushion", h_sitting_surface, dz_cushion)
seat_pos = grunk.action("seat_pos", "grocc::gp_Pnt", seat_x, seat_y, seat_z).output()
w_cushion = grunk.expression("w_cushion", "w_sitting_surface + w_armrest", w_sitting_surface, w_armrest)
seat_box = grunk.action("seat_box", "grocc::BRepPrimAPI_MakeBox", seat_pos, l_cushion, w_cushion, dz_cushion).output()

seat_fillet_radius_frontback = grunk.expression("seat_fillet_radius_frontback", "dz_cushion / 2.05", dz_cushion)
f1_i1 = grunk.Feature("f1_i1", "int", 1)
f1_i3 = grunk.Feature("f1_i3", "int", 3)
f1 = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["f1"], [seat_box]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e1"], [seat_box, f1_i1]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f1", seat_fillet_radius_frontback, "e1"]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e3"], [seat_box, f1_i3]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f1", seat_fillet_radius_frontback, "e3"])
    ],
    returns=["f1"]
).output()

seat_fillet_radius_leftright = grunk.Feature("seat_fillet_radius_leftright", "double", 0.01)
f2_i0 = grunk.Feature("f2_i0", "int", 0)
f2_i3 = grunk.Feature("f2_i3", "int", 3)
seat_cushion = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["seat_cushion"], [f1]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e1"], [f1, f2_i0]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["seat_cushion", seat_fillet_radius_leftright, "e1"]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e3"], [f1, f2_i3]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["seat_cushion", seat_fillet_radius_leftright, "e3"])
    ],
    returns=["seat_cushion"]
).output()
seat_cushion_recipe = grunk.Recipe(seat_cushion)

# define Back rest
backrest_posy = grunk.Feature("backrest_posy", "double", 0.)
backrest_posz = grunk.expression("backrest_posz", "h_sitting_surface - dz_cushion", h_sitting_surface, dz_cushion)
backrest_pos = grunk.action("backrest_pos", "grocc::gp_Pnt", l_cushion, backrest_posy, seat_z).output()
backrest_box = grunk.action("backrest_box", "grocc::BRepPrimAPI_MakeBox", backrest_pos, dx_back, w_sitting_surface, h_back).output()

backrest_radius_leftright = grunk.Feature("backrest_radius_leftright", "double", 0.1)
f3_i09 = grunk.Feature("f2_i0", "int", 9)
f3_i11 = grunk.Feature("f2_i3", "int", 11)
f3 = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["f3"], [backrest_box]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e09"], [backrest_box, f3_i09]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f3", backrest_radius_leftright, "e09"]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e11"], [backrest_box, f3_i11]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f3", backrest_radius_leftright, "e11"])
    ],
    returns=["f3"]
).output()
backrest_radius_top = grunk.expression("backrest_radius_top", "0.9 * dx_back", dx_back)
f4_i06 = grunk.Feature("f4_i06", "int", 6)
backrest = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["backrest"], [f3]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e06"], [f3, f4_i06]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["backrest", backrest_radius_top, "e06"]),
    ],
    returns=["backrest"]
).output()

# define head rest
headrest_posx = grunk.expression("headrest_posx", "l_cushion - t_headrest", l_cushion, t_headrest)
headrest_posz = grunk.expression("backrest_posz", "h_sitting_surface + h_back - dz_cushion - h_headrest - border_headrest", h_sitting_surface, h_back, dz_cushion, h_headrest, border_headrest)
headrest_width = grunk.expression("headrest_width", "w_sitting_surface - 2 * border_headrest", w_sitting_surface, border_headrest)
headrest_pos = grunk.action("headrest_pos", "grocc::gp_Pnt", headrest_posx, border_headrest, headrest_posz).output()
headrest_box = grunk.action("headrest_box", "grocc::BRepPrimAPI_MakeBox", headrest_pos, t_headrest, headrest_width, h_headrest).output()
headrest_radius = grunk.Feature("headrest_radius", "double", 0.08)
f5_i08 = grunk.Feature("f5_i08", "int", 8)
f5_i09 = grunk.Feature("f5_i09", "int", 9)
f5_i10 = grunk.Feature("f5_i10", "int",10)
f5_i11 = grunk.Feature("f5_i11", "int",11)
f5 = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["f5"], [headrest_box]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e08"], [headrest_box, f5_i08]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f5", headrest_radius, "e08"]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e09"], [headrest_box, f5_i09]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f5", headrest_radius, "e09"]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e10"], [headrest_box, f5_i10]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f5", headrest_radius, "e10"]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e11"], [headrest_box, f5_i11]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["f5", headrest_radius, "e11"])
    ],
    returns=["f5"]
).output()
headrest_radius2 = grunk.Feature("headrest_radius2", "double", 0.015)
f6_i0 = grunk.Feature("f6_i0", "int", 0)
headrest = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["headrest"], [f5]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e"], [f5, f6_i0]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["headrest", headrest_radius2, "e"]),
    ],
    returns=["headrest"]
).output()

# define back
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
rot_pnt_y = grunk.Feature("rot_pnt_y", "double", 0.)
rot_pnt = grunk.action("rot_pnt", "grocc::gp_Pnt", l_cushion, rot_pnt_y, h_sitting_surface).output()
#TODO wouldn't need to define double features, if gpgp_Dir_Ax had serialize/deserialze
rot_dir_x = grunk.Feature("rot_dir_x", "double", 0.)
rot_dir_y = grunk.Feature("rot_dir_y", "double", 1.)
rot_dir_z = grunk.Feature("rot_dir_z", "double", 0.)
rot_dir = grunk.action("rot_dir", "grocc::gp_Dir", rot_dir_x, rot_dir_y, rot_dir_z).output()
rot_ax = grunk.action("rot_ax", "grocc::gp_Ax1", rot_pnt, rot_dir).output()
backrest_reclined = grunk.script(
    [
        grunk.ScriptStep("grocc::gp_Trsf",["t"],[]),
        grunk.ScriptStep("grocc::gp_Trsf::SetRotation", [], ["t", rot_ax, phi_recline]),
        grunk.ScriptStep("grocc::BRepBuilderAPI_Transform", ["backrest_reclined"], [back, "t"])
    ],
    returns=["backrest_reclined"]

).output()
backrest_recipe = grunk.Recipe(backrest_reclined)


# define arm rest
armrest_posx = grunk.expression("armrest_posx", "l_cushion - l_armrest + dx_back * cos(phi_recline) / 2", l_cushion, l_armrest, dx_back, phi_recline)
# @bug: need to query the value of l_armrest, otherwise I get "value not initialized" error later on.
# l_armrest.value()
armrest_posy = grunk.expression("armrest_posy", "- w_armrest", w_armrest)
armrest_pos = grunk.action("armrest_pos", "grocc::gp_Pnt", armrest_posx, armrest_posy, h_sitting_surface).output()
armrest_dx = grunk.expression("armrest_dx", "l_armrest + h_armrest * sin(phi_recline)", l_armrest, h_armrest, phi_recline)
armrest_box = grunk.action("armrest_box", "grocc::BRepPrimAPI_MakeBox", armrest_pos, armrest_dx, w_armrest, h_armrest).output()
armrest_radius = grunk.Feature("armrest_radius", "double", 0.05)
f6_i01 = grunk.Feature("f6_i01", "int", 1)
armrest_nobop = grunk.script(
    [
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet", ["armrest_nobop"], [armrest_box]),
        grunk.ScriptStep("geo::internal::GetEdge", ["e"], [armrest_box, f6_i01]),
        grunk.ScriptStep("grocc::BRepFilletAPI_MakeFillet::Add", [], ["armrest_nobop", armrest_radius, "e"]),
    ],
    returns = ["armrest_nobop"]
).output()
pnt_x = grunk.expression("pnt_x", "l_cushion + dx_back * cos(phi_recline) / 2.0", l_cushion, dx_back, phi_recline)
pnt_y = grunk.Feature("pnt_y", "double", 0.)
dir_x = grunk.expression("dir_x", "cos(phi_recline)", phi_recline)
dir_y = grunk.Feature("dir_y", "double", 0.)
dir_z = grunk.expression("dir_z", "-sin(phi_recline)", phi_recline)
uvmin = grunk.Feature("uvmin", "double", 0.)
uvmax = grunk.Feature("uvmax", "double", 1.)
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
armrest_recipe = grunk.Recipe(armrest)

# define single seat
seat_recipe = grunk.Recipe()
seat_recipe.insert_recipe("seat_cushion", seat_cushion_recipe)
seat_recipe.insert_recipe("backrest", backrest_recipe)
seat_recipe.insert_recipe("armrest", armrest_recipe)
seat_recipe.recipe("seat_cushion", {"cushion" : "seat_cushion"}, {})
seat_recipe.recipe("backrest", {"back": "backrest_reclined"}, {})
seat_recipe.recipe("armrest", {"armrest": "armrest"}, {})
single_seat = grunk.script(
    [
        grunk.ScriptStep("grocc::TopoDS_Compound", ["single_seat"], []),
        grunk.ScriptStep("grocc::BRep_Builder", ["aBuilder"], []),
        grunk.ScriptStep("grocc::BRep_Builder::MakeCompound", [], ["aBuilder", "single_seat"]),
        grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "single_seat", seat_recipe["cushion"]]),
        grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "single_seat", seat_recipe["back"]]),
        grunk.ScriptStep("grocc::BRep_Builder::Add", [], ["aBuilder", "single_seat", seat_recipe["armrest"]]),
    ],
    returns = ["single_seat"]
).output()
seat_recipe.insert_feature(single_seat)

# define support
support_x = grunk.expression("support_x", "(l_cushion - dx_support) / 2", l_cushion, dx_support)
support_y = grunk.Feature("support_y", "double", 0.0)
support_z = grunk.Feature("support_z", "double", 0.0)
support_pos = grunk.action("support_pos", "grocc::gp_Pnt", support_x, support_y, support_z).output()
dz_support = grunk.expression("dz_support", "h_sitting_surface - dz_cushion", h_sitting_surface, dz_cushion)
support = grunk.action("support", "grocc::BRepPrimAPI_MakeBox", support_pos, dx_support, w_support, dz_support).output()


recipe.insert_recipe("seat", seat_recipe)
recipe.insert_recipe("support", grunk.Recipe(support))

# define seat row 


grunk.write("seats.grr", recipe)

filename = grunk.Feature("filename", "String", "cushion.brep")
grunk.action("", "grocc::BRepTools::Write", single_seat, filename).eval()