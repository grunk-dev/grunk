// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// Mockup of a future geoml/occt grunk plugin, built as its own shared object
// (see CMakeLists.txt's geoml_plugin target) rather than linked into the cad_autodiff
// executable. This is grunk-dev/grunk#235's "C++ plugin" kind: registration is plain
// C++ calling grunk::state::register_type/register_function directly, as opposed to
// a compiled Lua module like adtl.so (see src/main.cpp's load_adolc_plugin), which is
// grunk#235's "compiled Lua module" kind. main.cpp's load_geoml_plugin dlopen's this
// library and dlsym's grunk_geoml_plugin_entry_point below, the same way it dlopen's
// adtl.so and dlsym's luaopen_adtl - only this plugin's path needs to be known at
// cad_autodiff's build time, not its registration code.

#include <TColgp_Array1OfPnt.hxx>
#include <gp_Pnt.hxx>
#include <Geom_BezierCurve.hxx>

#include <geoml/curves/curves.h>
#include <geoml/surfaces/surfaces.h>
#include "geoml/data_structures/conversions.h"

#include <grunk/grunk.hpp>

#include "occt_sol_traits.hpp"

// The symbol main.cpp's load_geoml_plugin_entry_point looks up via dlsym. extern "C"
// gives it stable, unmangled linkage - the same requirement a Lua module's luaopen_*
// entry point has, just for a plain C++ function taking a grunk::state& instead of a
// lua_CFunction.
extern "C" void grunk_geoml_plugin_entry_point(grunk::state& grunk)
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
