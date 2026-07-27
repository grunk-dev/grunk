// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include <Geom_BezierCurve.hxx>
#include <BRepTools.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>

#include <sol/types.hpp>
#include <grunk/grunk.hpp>

#include <dlfcn.h>
#include <stdexcept>

#include "adolc/adtl.h"
#include "occt_sol_traits.hpp"

// This example's main point: algorithmic differentiation (AD) is possible *through*
// grunk's dynamic layer, not just around it - a value can flow through a grunk recipe
// (dependency tracking, lazy evaluation, YAML serialization) and still carry its AD
// tape/derivative information. It builds this up in three stages:
//
//   1. write_autodiff_only_recipe / read_autodiff_only_recipe - a recipe built purely
//      on ADOL-C's adouble type (see load_adolc_plugin), no CAD geometry involved.
//      Shows that grunk's dynamic layer is transparent to an AD type.
//   2. write_cad_only_recipe / read_cad_only_recipe - a recipe built on geoml/OCCT
//      geometry (see load_geoml_plugin), using plain doubles - no AD. Shows that
//      grunk's plugin mechanism can wrap a genuine, non-scripting-oriented C++
//      library, not just something already Lua-friendly like a SWIG-Lua module.
//   3. Not yet implemented (planned follow-up, see README.md): the *same* CAD recipe
//      from stage 2, but with geoml_plugin.so swapped for a geoml_adolc_plugin.so
//      built against ADOL-C, differentiating the geometry construction itself. This
//      is the payoff - stages 1 and 2 in one recipe - and the reason this whole
//      example is named "cad_autodiff".

// adtl.so is a SWIG-generated Lua module wrapping ADOL-C's tapeless adouble type
// (see https://gitlab.dlr.de/dlr-sp/occt-differentiation/swig-adol-c, lua_wrapper
// branch). Proof of concept for runtime plugin loading: rather than linking adtl.so
// into this executable and forward-declaring its luaopen_adtl entry point, it is
// dlopen'd from disk at startup and the entry point is looked up by name - the same
// thing linking + an extern "C" declaration would give us, just resolved at runtime
// instead of build time. ADTL_SO_PATH is set by CMake to wherever adtl.so was found.
lua_CFunction load_adtl_entry_point()
{
    void* handle = dlopen(ADTL_SO_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        throw std::runtime_error(std::string("could not load adtl.so: ") + dlerror());
    }

    dlerror(); // clear any prior error, per dlsym's own documented idiom for telling a
               // valid NULL result apart from a real lookup failure
    void* sym = dlsym(handle, "luaopen_adtl");
    if (char const* err = dlerror(); err != nullptr) {
        throw std::runtime_error(std::string("could not find luaopen_adtl in adtl.so: ") + err);
    }

    return reinterpret_cast<lua_CFunction>(sym);
}

// geoml_plugin.so is a mockup of a genuine C++ grunk plugin (see
// plugins/geoml/geoml_plugin.cpp): unlike adtl.so, a compiled Lua module loaded via
// load_compiled_plugin/luaL_requiref, it exposes a plain C++ entry point that
// registers types/functions directly against a grunk::state (grunk-dev/grunk#235's
// "C++ plugin" kind). Its signature is this library's own convention, not a Lua one -
// there is no lua_CFunction involved. It is dlopen'd here for the same reason adtl.so
// is: only its path needs to be known at build time (see CMakeLists.txt's
// geoml_plugin target), not the plugin itself. GEOML_PLUGIN_SO_PATH is set by CMake
// to the built geoml_plugin library's path.
using geoml_plugin_entry_point_t = void (*)(grunk::state&);

geoml_plugin_entry_point_t load_geoml_plugin_entry_point()
{
    // RTLD_GLOBAL (unlike adtl.so's RTLD_LOCAL above) is required here: this plugin's
    // registration code instantiates the same sol2 usertype machinery for OCCT types
    // (e.g. Geom_BezierCurve, via occt_sol_traits.hpp) as this executable does when
    // reading a value back out of a recipe (see read_cad_only_recipe's .as<Handle(...)>() calls).
    // Those are header-only template instantiations, compiled separately into this
    // executable and into geoml_plugin.so; without RTLD_GLOBAL (and cad_autodiff's own
    // ENABLE_EXPORTS, see CMakeLists.txt) the dynamic linker keeps the two copies'
    // sol2-internal type identities apart, so a Handle built by the plugin silently
    // fails to convert back to the "same" type on this side. adtl.so needs none of
    // this, since ADOL-C's adouble crosses the boundary only as an opaque grunk::object.
    void* handle = dlopen(GEOML_PLUGIN_SO_PATH, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        throw std::runtime_error(std::string("could not load geoml_plugin.so: ") + dlerror());
    }

    dlerror();
    void* sym = dlsym(handle, "grunk_geoml_plugin_entry_point");
    if (char const* err = dlerror(); err != nullptr) {
        throw std::runtime_error(std::string("could not find grunk_geoml_plugin_entry_point in geoml_plugin.so: ") + err);
    }

    return reinterpret_cast<geoml_plugin_entry_point_t>(sym);
}

void load_geoml_plugin(grunk::state& grunk)
{
    auto entry_point = load_geoml_plugin_entry_point();
    entry_point(grunk);
}

void load_adolc_plugin(grunk::state& grunk)
{
    // Load the compiled SWIG-Lua module as a grunk plugin: its own table becomes the
    // "adtl" namespace in original_env, its free functions (tan, exp, log, sqrt, pow,
    // ...) are made grunk-tracked, and its identity is recorded in grunk.plugins().
    grunk::PluginInfo info{"adtl", "2.7.2"}; // matches the wrapped ADOL-C release
    sol::table adtl = grunk.load_compiled_plugin(info, load_adtl_entry_point());

    // No .add_member_function calls are needed: adouble's default constructor lets
    // register_external_type probe an instance and discover setADValue/getADValue/
    // getValue (and any other method actually used) lazily, the first time each is
    // looked up - SWIG-Lua gives no way to enumerate them up front.
    sol::table adouble_static = adtl["adouble"];
    grunk.register_external_type("adtl.adouble", adouble_static, adtl);

    // adouble.i explicitly `%ignore`s operator<<, so the SWIG binding gives adouble
    // instances no __tostring - without one, grunk can't serialize an adouble held
    // directly by a Feature (e.g. one built via new_feature) into recipe YAML at all.
    // Bridge one here in ctor syntax, so Serializer's ctor_syntax_to_new_feature_syntax
    // can turn a written-out parameter back into a "new_feature" call on read-back,
    // exactly the convention grunk-registered types use (see grunk's own test suite,
    // where MyScalar's tostring returns "MyScalar.new(...)" for the same reason).
    sol::table adouble_type = grunk.get_type("adtl.adouble");
    sol::protected_function ctor = adouble_type["new"];
    sol::object probe = ctor();

    // Looking "getValue" up on the (undecorated) type table triggers register_external_type's
    // auto-discovery probing and caches it as a plain, un-tracked function_meta - exactly
    // what is needed here, since a tostring metamethod must run synchronously and must not
    // itself create a dependency-tracked action.
    sol::protected_function get_value = adouble_type["getValue"];

    sol::state_view lua(adouble_static.lua_state());
    sol::protected_function getmetatable = lua["getmetatable"];
    sol::table adouble_meta = getmetatable(probe);

    sol::object tostring_fn = sol::make_object(lua, sol::as_function(
        [get_value](sol::object self) -> std::string {
            double value = get_value(self).get<double>();
            return "adtl.adouble.new(" + grunk::to_string(value) + ")";
        }
    ));
    adouble_meta.set(sol::meta_function::to_string, tostring_fn);

    // SWIG-Lua's per-instance __index is a C dispatch function (SWIG_Lua_class_get),
    // not a plain table - so an ordinary `instance["__tostring"]` lookup never reaches
    // the metatable's own raw fields the way it would for a table-based __index. That is
    // exactly the check grunk::serialize does before invoking the real tostring
    // metamethod (which bypasses __index entirely and works fine on its own - confirmed
    // empirically), so without this, grunk reports no tostring even though one exists.
    // Wrap __index so that one lookup succeeds too, while every other key still falls
    // through to SWIG's original dispatcher unchanged.
    sol::protected_function original_index = adouble_meta[sol::meta_function::index];
    adouble_meta.set_function(sol::meta_function::index, [original_index, tostring_fn](sol::object self, std::string const& key) -> sol::object {
        if (key == "__tostring") {
            return tostring_fn;
        }
        sol::protected_function_result res = original_index(self, key);
        return res.valid() ? sol::object(res) : sol::lua_nil;
    });
}

// Stage 1: AD only, no CAD - see the file-level comment above.
void write_autodiff_only_recipe()
{
    auto grunk = grunk::state();
    load_adolc_plugin(grunk);

    auto recipe = grunk.create_recipe();
    recipe.insert_module_script(
        "me",
        R"(function seed(a)
   ret = adtl.adouble.new(a)
   ret:setADValue(0,1.)
   return ret
end)");
    recipe.eval(R"(
        x_val = grunk.feature(5.)
        x = me.seed(x_val)

        y = adtl.adouble.new(17)

        z = x * y
        w = adtl.sin(z*6)
    )");
    recipe.tag_features();

    grunk.write("test_adolc.grr.yml", recipe);
}

void read_autodiff_only_recipe()
{
    auto grunk = grunk::state();
    load_adolc_plugin(grunk);

    auto recipe = grunk.read("test_adolc.grr.yml");

    sol::object w = recipe.get_feature("w").value();
    sol::table adouble_type = grunk.get_type("adtl.adouble");
    sol::protected_function get_value = adouble_type["getValue"];
    sol::protected_function get_ad_value = adouble_type["getADValue"];

    double value = get_value(w).get<double>();
    double derivative = get_ad_value(w, 0).get<double>();

    std::cout << "w = " << value << ", dw/dx = " << derivative << std::endl;
}

// Stage 2: CAD only, no AD - see the file-level comment above. Builds a Gordon
// fuselage cross-section out of bezier curves (front/back profiles, upper/lower
// guides), a small enough recipe to keep the AD-vs-no-AD contrast readable.
void write_cad_only_recipe()
{
    auto grunk = grunk::state();
    load_geoml_plugin(grunk);

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
    )");
    recipe.tag_features();

    grunk.write("gordon.grr.yml", recipe);
}

void read_cad_only_recipe()
{
    auto grunk = grunk::state();
    load_geoml_plugin(grunk);

    auto recipe = grunk.read("gordon.grr.yml");

    auto front_profile = recipe.get_feature("front_profile").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(front_profile), "front_profile.brep");

    auto back_profile = recipe.get_feature("back_profile").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(back_profile), "back_profile.brep");

    auto lower_guide = recipe.get_feature("lower_guide").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(lower_guide), "lower_guide.brep");

    auto upper_guide = recipe.get_feature("upper_guide").value().as<Handle(Geom_BezierCurve)>();
    BRepTools::Write(BRepBuilderAPI_MakeEdge(upper_guide), "upper_guide.brep");
}

int main() {

    std::cout << "Hello from cad_autodiff example!" << std::endl;

    std::cout << "\n[1/3] AD only (adtl.so): a grunk recipe built on ADOL-C's adouble type,\n"
                 "      no CAD geometry involved." << std::endl;
    write_autodiff_only_recipe();
    read_autodiff_only_recipe();

    std::cout << "\n[2/3] CAD only (geoml_plugin.so): the same kind of recipe, now built on\n"
                 "      geoml/OCCT geometry with plain doubles - no AD." << std::endl;
    write_cad_only_recipe();
    read_cad_only_recipe();

    std::cout << "\n[3/3] CAD + AD: not yet implemented here - see README.md." << std::endl;

    std::cout << "\nDone." << std::endl;
    return 0;
}
