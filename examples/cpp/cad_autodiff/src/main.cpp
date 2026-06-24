// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>

#include <geoml/curves/curves.h>
#include <geoml/surfaces/surfaces.h>
#include "geoml/data_structures/conversions.h"

#include <adolc/adtl.h>

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

    //grunk.register_type<Geom_Curve, sol::automagic_flags::none>("Geom_Curve")
    //.with_std_vector();

    //grunk.register_type<Geom_BezierCurve, sol::automagic_flags::none>("Geom_BezierCurve")
    //.add_bases<Geom_Curve>();

    //grunk.register_type<Geom_Surface>("Geom_Surface");

    //grunk.register_type<Geom_BSplineSurface>("Geom_BSplineSurface")
    //.add_bases<Geom_Surface>();

    grunk.register_function(
        "bezier_curve",
        [](std::vector<gp_Pnt> const& poles) -> Handle(Geom_BezierCurve) {
            TColgp_Array1OfPnt occ_poles = geoml::StdVector_to_TCol(poles);
            return new Geom_BezierCurve(occ_poles);
        }
    );

    grunk.register_function("interpolate_curve_network", geoml::interpolate_curve_network);
}

void register_adolc(grunk::state& grunk)
{
    using namespace adtl;
    grunk.register_type<adouble>("adouble")
	.add_constructors(
		[](double x) {return adouble(x);}
	)
	.add_member_function(
		"setADValue", [](adouble& self, const unsigned int idx, const double ad_value){ self.setADValue(idx, ad_value); }
	)
	.add_member_function(
		"getADValue", [](adouble& self, const unsigned int idx){ return self.getADValue(idx); }
	)
	.add_member_function(
                "__mul", [](adouble const& l, double const&r){ return l * r; }
	);

    grunk.register_function("initialize_adouble", [](double v) { 
		    adouble res(v);
		    res.setADValue(0, 1.);
		    return res;
    });

	
}

void write_recipe_ad()
{
    auto grunk = grunk::state();
    register_adolc(grunk);

    auto recipe = grunk.create_recipe();
    recipe.eval(R"(
        y = grunk.feature(5.)
	-- x = adouble.new(y)
	-- x:as(adouble).setADValue(0, 1.) -- TODO: Jan check why this syntax does not work
        -- adouble.setADValue(x, 0, 1.)

        x = initialize_adouble(y)

	a = 6.
	output = x * a
	resultAD = adouble.getADValue(output, 0)
    )");
    recipe.tag_features();

    grunk.write("test_adolc.grr.yml", recipe);
}

void write_recipe()
{
    auto grunk = grunk::state();
    register_geoml(grunk);

    auto recipe = grunk.create_recipe();
    recipe.eval(R"(
        X = grunk.feature(-4600.)

        P_1_y = grunk.feature(0.)
        P_1_z = grunk.feature(1950.)
        P_1 = gp_Pnt.new(X, P_1_y, P_1_z)

        P_2_y = grunk.feature(-1076.95526217)
        P_2_z = grunk.feature(1950.)
        P_2 = gp_Pnt.new(X, P_2_y, P_2_z)

        P_3_y = grunk.feature(-1950.)
        P_3_z = grunk.feature(1076.95526217)
        P_3 = gp_Pnt.new(X, P_3_y, P_3_z)

        P_4_y = grunk.feature(-1950.)
        P_4_z = grunk.feature(0.)
        P_4 = gp_Pnt.new(X, P_4_y, P_4_z)

        P_5_y = grunk.feature(-1950.)
        P_5_z = grunk.feature(-1076.95526217)
        P_5 = gp_Pnt.new(X, P_5_y, P_5_z)

        P_6_y = grunk.feature(-1076.95526217)
        P_6_z = grunk.feature(-1950.)
        P_6 = gp_Pnt.new(X, P_6_y, P_6_z)

        P_7_y = grunk.feature(0.)
        P_7_z = grunk.feature(-1950.)
        P_7 = gp_Pnt.new(X, P_7_y, P_7_z)

        front_poles = gp_Pnt.as_vec(P_1, P_2, P_3, P_4, P_5, P_6, P_7)
        front_profile = bezier_curve(front_poles)

        P_back_1 = gp_Pnt.new(12500., 0., 1950.)
        P_back_2 = gp_Pnt.new(12500., -1076.95526217, 1950.);
        P_back_3 = gp_Pnt.new(12500., -1950., 1076.95526217)
        P_back_4 = gp_Pnt.new(12500., -1950., 0.)
        P_back_5 = gp_Pnt.new(12500., -1950., -1076.95526217)
        P_back_6 = gp_Pnt.new(12500., -1076.95526217, -1950.)
        P_back_7 = gp_Pnt.new(12500., 0., -1950.)

        back_poles = gp_Pnt.as_vec(P_back_1, P_back_2, P_back_3, P_back_4, P_back_5, P_back_6, P_back_7)
        back_profile = bezier_curve(back_poles)

        upper_poles = gp_Pnt.as_vec(P_1, P_back_1)
        upper_guide = bezier_curve(upper_poles)

        lower_poles = gp_Pnt.as_vec(P_7, P_back_7)
        lower_guide = bezier_curve(lower_poles)

        -- profiles = Geom_Curve.as_vec(front_profile, back_profile)
        -- guides = Geom_BezierCurve.as_vec(upper_guide, lower_guide)

        -- middle_fuselage = interpolate_curve_network(profiles, guides, 1.)

    )");
    recipe.tag_features();

    grunk.write("gordon.grr.yml", recipe);
}

void read_recipe_ad()
{
    using namespace adtl;
    auto grunk = grunk::state();
    register_adolc(grunk);

    auto recipe = grunk.read("test_adolc.grr.yml");

    //auto front_profile = recipe.get_feature("x").value().as<adouble>().getValue();
    auto front_profile = recipe.get_feature("resultAD").value().as<double>();

    std::cout << "AD value: " << front_profile << std::endl;
}


void read_recipe()
{
    auto grunk = grunk::state();
    register_geoml(grunk);

    auto recipe = grunk.read("gordon.grr.yml");

    auto front_profile = recipe.get_feature("front_profile").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(front_profile), "front_profile.brep");

    auto back_profile = recipe.get_feature("back_profile").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(back_profile), "back_profile.brep");

    auto lower_guide = recipe.get_feature("lower_guide").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(lower_guide), "lower_guide.brep");

    auto upper_guide = recipe.get_feature("upper_guide").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(upper_guide), "upper_guide.brep");


    /*
    my_func = [&recipe](auto const& x) {
        recipe.get_feature("X").set_value(x);
	gp_Pnt pnt;
	return recipe.get_feature("front_profile").value().as<Handle(Geom_BezierCurve)>()->D0(0., pnt);
	return pnt.X();
    };
    */

    /*
    auto middle_fuselage_f = recipe.get_feature("middle_fuselage");
    std::cout << "wtf\n";
    auto middle_fuselage_obj = middle_fuselage_f.value();
    std::cout << "shit...\n";
    auto middle_fuselage = middle_fuselage_obj.as<Handle(Geom_BSplineSurface)>();
    std::cout << "Handle is Null? " << middle_fuselage.IsNull() << "\n";
    std::string filename = "middle_fuselage.brep";
    BRepTools::Write(BRepBuilderAPI_MakeFace(middle_fuselage, Precision::Confusion()), filename.c_str());
    */
}

int main() {

    std::cout << "Hello from cad_autodiff example!" << std::endl;

    // create a new grunk state, "load" occt and geoml plugins and write
    // a recipe for gordon surface creation.
    write_recipe_ad();

    // read the recipe from file and execute the steps
    read_recipe_ad();

    std::cout << "Done." << std::endl;
    return 0;
}
