// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
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

#include <sol/types.hpp>
#include <grunk/grunk.hpp>

#include <dlfcn.h>
#include <stdexcept>

#include "adolc/adtl.h"

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

void write_recipe_ad()
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

void read_recipe_ad()
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
