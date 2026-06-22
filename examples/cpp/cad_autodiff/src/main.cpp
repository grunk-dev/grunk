// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>

#include <geoml/curves/curves.h>
#include <geoml/surfaces/surfaces.h>
#include "geoml/data_structures/conversions.h"

#include <sol/types.hpp>
#include <grunk/grunk.hpp>

namespace sol {
    template <typename T>
    struct unique_usertype_traits<opencascade::handle<T>> {
        using type=T;
        using actual_type=opencascade::handle<T>;
        static const bool value = true;

        static bool is_null(const actual_type& ptr) {
            return ptr.IsNull();
        }

        static type* get (const actual_type& ptr) {
            return ptr.get();
        }
    };
}

/**
 * This function mimics the behavior of a future geoml/occt plugin.
 * Function and type registration would be performed in a 
 * grunk plugin src code compiled to a shared object and then 
 * registration is done at plugin load time.
 */
void register_geoml(grunk::state& grunk)
{
    grunk.register_type<gp_Pnt>("gp_Pnt")
    .add_constructors(
        [](Standard_Real x, Standard_Real y, Standard_Real z) { return gp_Pnt(x,y,z); }
    )
    .with_std_vector();

    grunk.register_type<Handle(Geom_BezierCurve)>("Handle_Geom_BezierCurve")
    .with_std_vector();

    grunk.register_function(
        "Geom_BezierCurve",
        [](std::vector<gp_Pnt> const& poles) -> Handle(Geom_BezierCurve) {
            TColgp_Array1OfPnt occ_poles = geoml::StdVector_to_TCol(poles);
            return new Geom_BezierCurve(occ_poles);
        }
    );

    grunk.register_function("interpolate_curve_network", geoml::interpolate_curve_network);
}

int main() {

    std::cout << "Hello from cad_autodiff example!" << std::endl;

    auto grunk = grunk::state();
    register_geoml(grunk);

    auto recipe = grunk.create_recipe();
    recipe.eval(R"(
        X = -4600.

        P_1_y = 0.
        P_1_z = 1950.
        P_1 = gp_Pnt.new(X, P_1_y, P_1_z)

        P_2_y = -1076.95526217
        P_2_z = 1950.
        P_2 = gp_Pnt.new(X, P_2_y, P_2_z)

        P_3_y = -1950.
        P_3_z = 1076.95526217
        P_3 = gp_Pnt.new(X, P_3_y, P_3_z)

        P_4_y = -1950.
        P_4_z = 0.
        P_4 = gp_Pnt.new(X, P_4_y, P_4_z)

        P_5_y = -1950.
        P_5_z = -1076.95526217
        P_5 = gp_Pnt.new(X, P_5_y, P_5_z)

        P_6_y = -1076.95526217
        P_6_z = -1950.
        P_6 = gp_Pnt.new(X, P_6_y, P_6_z)

        P_7_y = 0.
        P_7_z = -1950.
        P_7 = gp_Pnt.new(X, P_7_y, P_7_z)

        front_poles = gp_Pnt.as_vec(P_1, P_2, P_3, P_4, P_5, P_6, P_7)
        front_profile = Geom_BezierCurve(front_poles)

        P_back_1 = gp_Pnt.new(12500., 0., 1950.)
        P_back_2 = gp_Pnt.new(12500., -1076.95526217, 1950.);
        P_back_3 = gp_Pnt.new(12500., -1950., 1076.95526217)
        P_back_4 = gp_Pnt.new(12500., -1950., 0.)
        P_back_5 = gp_Pnt.new(12500., -1950., -1076.95526217)
        P_back_6 = gp_Pnt.new(12500., -1076.95526217, -1950.)
        P_back_7 = gp_Pnt.new(12500., 0., -1950.)

        back_poles = gp_Pnt.as_vec(P_back_1, P_back_2, P_back_3, P_back_4, P_back_5, P_back_6, P_back_7)
        back_profile = Geom_BezierCurve(back_poles)

        upper_poles = gp_Pnt.as_vec(P_1, P_back_1)
        upper_guide = Geom_BezierCurve(upper_poles)

        lower_poles = gp_Pnt.as_vec(P_7, P_back_7)
        lower_guide = Geom_BezierCurve(lower_poles)

        profiles = Handle_Geom_BezierCurve.as_vec(front_profile, back_profile)
        -- guides = Handle_Geom_BezierCurve.as_vec(upper_guide, lower_guide)

        -- middle_fuselage = interpolate_curve_network(profiles, guides, 1.)

    )");

    grunk.write("gordon.grr.yml", recipe);
    
    auto middle_fuselage = recipe.get_feature("middle_fuselage").value().as<Handle(Geom_BSplineSurface)>();
    std::string filename = "middle_fuselage.brep";
    BRepTools::Write(BRepBuilderAPI_MakeFace(middle_fuselage, Precision::Confusion()), filename.c_str());


    
    std::cout << "Done." << std::endl;
    return 0;
}