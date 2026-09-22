// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include <sol/types.hpp>
#include <grunk/grunk.hpp>

// Point of this example: algorithmic differentiation (AD) works *through* grunk's
// dynamic layer, not just around it - a value flows through a grunk recipe
// (dependency tracking, lazy evaluation, YAML serialization) and still carries its AD
// derivative. Three stages, run in order by main():
//
//   1. write_autodiff_only_recipe / read_autodiff_only_recipe - ADOL-C's adouble
//      type, no CAD. Shows grunk's dynamic layer is transparent to an AD type.
//   2. write_cad_recipe(false) / read_cad_recipe - geoml/OCCT geometry (see
//      load_geoml_plugin), plain doubles, no AD. Shows grunk's plugin mechanism can
//      wrap a real C++ library, not just something already Lua-friendly.
//   3. write_cad_recipe(true) / read_recipe_ad - the exact same recipe script as
//      stage 2 (with_ad only changes which plugin loads), but with AD-carrying
//      geometry - see README.md's "Stage 3" section. This is the payoff (stages 1+2
//      combined) and why the example is named "cad_autodiff".
//
// Deliberately no OCCT/geoml headers below: everything CAD-related is confined to the
// geoml plugins (plugins/geoml_registration.hpp) - this file only ever sees
// grunk::state, recipes, and sol::object/bool.

// adtl_plugin.so wraps adtl.so, a SWIG-generated Lua module wrapping ADOL-C's
// tapeless adouble type (see plugins/adtl/adtl_plugin.cpp) - grunk-dev/grunk#235's
// "compiled Lua" plugin kind. Loaded the same way as geoml_plugin.so, via
// grunk::plugin::load_native.
void load_adolc_plugin(grunk::state& grunk)
{
    grunk::plugin::load_native(grunk, ADTL_PLUGIN_SO_PATH);
}

// geoml_plugin.so is a genuine C++ grunk plugin (see plugins/geoml/geoml_plugin.cpp):
// unlike adtl.so (a compiled Lua module, loaded via load_compiled_plugin), its entry
// point registers types/functions directly against a grunk::state -
// grunk-dev/grunk#235's "C++ plugin" kind. Loaded the same way as adtl_plugin.so, via
// grunk::plugin::load_native - only its path needs to be known at build time (see
// CMakeLists.txt's geoml_plugin target), not the plugin's own code.
void load_geoml_plugin(grunk::state& grunk)
{
    grunk::plugin::load_native(grunk, GEOML_PLUGIN_SO_PATH);
}

// Mechanically identical to load_geoml_plugin above, just pointed at a different
// library - see plugins/geoml_adolc/geoml_adolc_plugin.cpp and README.md's "Stage 3"
// section. plugins/geoml_adolc/ is a separate, standalone CMake project you build
// yourself (see that directory's CMakeLists.txt for why), so this throws unless
// GEOML_ADOLC_PLUGIN_SO_PATH (a CMake cache variable, ../../CMakeLists.txt) is
// pointed at a real build of it.
void load_geoml_adolc_plugin(grunk::state& grunk)
{
    grunk::plugin::load_native(grunk, GEOML_ADOLC_PLUGIN_SO_PATH);
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

// Stages 2 and 3 share this recipe verbatim - with_ad only changes which plugin
// loads, never the recipe text (the whole point, see the file comment above).
//
// Just one bezier curve, not a full CAD model - a dev artifact for exercising the
// plugin swap. X is a named feature (not a literal) so there's a single input for
// stage 3 to seed a derivative from - see read_recipe_ad below.
//
// point_at_half/x_at_half/y_at_half sample the curve at u=0.5 (Geom_BezierCurve's
// Value, then gp_Pnt's X()/Y() accessors - both genuine geoml_registration.hpp
// primitives, not example-specific) and x_at_half/y_at_half are declared as the recipe's named
// outputs (grunk-dev/grunk#266's `outputs:` block). Both work under either plugin,
// so this is safe to bake into the shared recipe text; only the AD-only derivative
// extraction stays confined to stage 3's read_recipe_ad, appended after read (via
// its own "ad" Lua module) rather than written here.
//
// curve:Value(0.5)/point_at_half:X()/:Y(), not the qualified geoml.Geom_BezierCurve.Value(curve, u)/
// geoml.gp_Pnt.X(point_at_half)/geoml.gp_Pnt.Y(point_at_half) form: grunk's native colon-call
// dispatch (grunk-dev/grunk#269) resolves a feature's method via a type hint stamped
// on it at construction time (never by evaluating it), so an ordinary-looking
// `instance:method(...)` works directly for any feature that carries one - this is
// the default, recommended form (see docs/usage.rst), and unaffected by geoml's own
// registrations living under the "geoml" namespace table (see geoml_registration.hpp)
// rather than flat, since colon-call resolves via a C++-type-keyed registry, not a
// Lua-side name lookup. bezier_curve itself returns Handle(Geom_BezierCurve) (i.e.
// opencascade::handle<Geom_BezierCurve>), which needed a small grunk fix
// (function_meta.hpp's deduce_return_type_hint recognizing
// sol::unique_usertype_traits<R> and hinting the pointee type, not the smart-pointer
// wrapper itself) before curve's own colon-call would resolve.
void write_cad_recipe(bool with_ad)
{
    auto grunk = grunk::state();
    if (with_ad) {
        load_geoml_adolc_plugin(grunk);
    } else {
        load_geoml_plugin(grunk);
    }

    auto recipe = grunk.create_recipe();
    recipe.eval(R"(
        X = grunk.feature(1.)

        P_1 = geoml.gp_Pnt.new(X, 0., 0.)
        P_2 = geoml.gp_Pnt.new(1., 2., 0.)
        P_3 = geoml.gp_Pnt.new(2., -1., 0.)
        P_4 = geoml.gp_Pnt.new(3., 0., 0.)

        poles = geoml.gp_Pnt.as_vec(P_1, P_2, P_3, P_4)
        curve = geoml.bezier_curve(poles)

        point_at_half = curve:Value(0.5)
        x_at_half = point_at_half:X()
        y_at_half = point_at_half:Y()
    )");
    recipe.tag_features();
    recipe.insert_output("x(0.5)", "x_at_half");
    recipe.insert_output("y(0.5)", "y_at_half");

    grunk.write("bezier_curve.grr.yml", recipe);
}

// Stage 2 read-back: verifies the curve rebuilds via the (non-AD) plugin, and exports
// it - via a registered function called from within the recipe, not C++ code in this
// file - so this stays as OCCT-agnostic as write_cad_recipe above.
void read_cad_recipe()
{
    auto grunk = grunk::state();
    load_geoml_plugin(grunk);

    auto recipe = grunk.read("bezier_curve.grr.yml");
    recipe.eval(R"(exported = geoml.export_brep(curve, "bezier_curve.brep"))");

    bool exported = recipe.get_feature("exported").value().as<bool>();
    std::cout << "bezier_curve.brep exported: " << std::boolalpha << exported << std::endl;

    // x(0.5)/y(0.5) (write_cad_recipe) are recipe outputs (grunk-dev/grunk#266),
    // retrievable by name without knowing the internal Lua variable that produced
    // them (x_at_half/y_at_half) - same values read_recipe_ad reads back below,
    // just without any derivative since no AD plugin is loaded here.
    double curve_x = recipe.get_output("x(0.5)").value().as<double>();
    double curve_y = recipe.get_output("y(0.5)").value().as<double>();
    std::cout << "curve point x(0.5) = " << curve_x << ", y(0.5) = " << curve_y << std::endl;
}

// Stage 3: reads bezier_curve.grr.yml (written by write_cad_recipe above) with the
// AD plugin loaded instead of the plain one - no separate AD write needed, since the
// recipe re-executes its steps at read time (grunk::environment::eval) against
// whichever plugin is currently loaded.
//
// The point of this function: a grunk recipe can be treated as a black-box function
// from parameters to outputs. At the call site - here - that means never reaching
// into the recipe's internal Lua variable names (X's constructing geoml.gp_Pnt.new call,
// point_at_half, x_at_half/y_at_half, ...): only its declared parameter ("X") and
// its declared outputs ("x(0.5)"/"y(0.5)", grunk-dev/grunk#266) are ever named.
// Forward-mode AD fits this exactly - set a seed on a parameter, then read both the
// primal and the dual (derivative) value off any output, without caring how the
// recipe's steps get from one to the other internally.
void read_recipe_ad()
{
    // By this point read_cad_recipe() (stage 2, called from main() just before this)
    // has already dlopen'd geoml_plugin.so - and with it, stock OCCT - into this same,
    // never-dlclose'd process. If adOCCT keeps the same SONAMEs as the stock OCCT it
    // forks, the dynamic linker may resolve geoml_adolc_plugin.so's OCCT dependencies
    // against those already-mapped stock objects instead of the AD-enabled ones, no
    // matter how LD_LIBRARY_PATH is set - see README.md's "Stage 3: CAD + AD" section
    // ("Building it yourself") for the full hazard and how to rule it out if the
    // derivative check below ever starts failing.
    auto grunk = grunk::state();
    load_geoml_adolc_plugin(grunk);

    auto recipe = grunk.read("bezier_curve.grr.yml");

    // "ad" composes genuine, general-purpose Standard_Real operations
    // (geoml_registration.hpp: construction, getValue, getADValue, setADValue)
    // into the seed/primal/derivative vocabulary the black-box view below needs.
    // It's a Lua module script inserted directly into the recipe
    // (Recipe::insert_module_script, the same mechanism stage 1 uses for "me" -
    // write_autodiff_only_recipe), not a plugin-registered function: the plugin
    // only knows about OCCT/geoml/ADOL-C itself, not this recipe's particular use
    // of it. None of "ad"'s three functions name a parameter or output either -
    // they operate on whatever Standard_Real value they're given.
    recipe.insert_module_script("ad", R"(
        function seed(x, value)
            ret = geoml.Standard_Real.new(x)
            geoml.Standard_Real.setADValue(ret, 0, value)
            return ret
        end

        function primal(x)
            return geoml.Standard_Real.getValue(x)
        end

        function derivative(x, direction)
            return geoml.Standard_Real.getADValue(x, direction)
        end
    )");
    sol::table ad = recipe.get<sol::table>("ad");
    sol::protected_function ad_seed = ad["seed"];
    sol::protected_function ad_primal = ad["primal"];
    sol::protected_function ad_derivative = ad["derivative"];

    // ad.seed/ad.primal/ad.derivative each call genuinely tracked functions
    // internally (Standard_Real.new/getValue/getADValue/setADValue), so calling
    // any of them (whether via eval or, as here, a direct sol2 call) yields a
    // lazy DynamicFeature, not the underlying value directly - this unwraps
    // that one evaluation.
    auto unwrap = [](sol::object o) -> sol::object {
        return o.is<grunk::DynamicFeature>() ? o.as<grunk::DynamicFeature>().value() : o;
    };

    // --- Parameter side: seed X's forward AD direction. ---
    //
    // A seed of 1. reads a plain d(.)/dX derivative back below; any other value
    // would scale the resulting derivative linearly, same as ADOL-C's setADValue
    // itself. Seeding can't happen inside the recipe's own steps: X's value
    // comes from the YAML (a plain number) and write_cad_recipe's own
    // `P_1 = gp_Pnt.new(X, 0., 0.)` step already runs (lazily, but wired into the
    // DAG) against that same X node before any step appended here would run.
    // Re-assigning the Lua variable X wouldn't reach it either - P_1 depends on
    // the specific DynamicFeature object already in the environment, not on
    // whatever the name "X" happens to point to afterwards. So this seeds X's
    // *existing* node in place via DynamicFeature::set_value, which invalidates
    // P_1/curve (and, transitively, the outputs queried below) exactly like
    // changing any other feature's value would.
    grunk::DynamicFeature X = recipe.get_feature("X");
    double x_val = X.value().as<double>();
    X.set_value(unwrap(ad_seed(x_val, 1.)));

    // --- Output side: read primal + dual value off each declared output. ---
    //
    // recipe.get_output resolves "x(0.5)"/"y(0.5)" to whatever internal feature
    // write_cad_recipe tagged them to (x_at_half/y_at_half) - this code never
    // needs to know that mapping itself. Forcing .value() here (after X has
    // been seeded above) re-evaluates the recipe's steps under the new seed.
    sol::object x_output = recipe.get_output("x(0.5)").value();
    sol::object y_output = recipe.get_output("y(0.5)").value();

    double curve_x = unwrap(ad_primal(x_output)).as<double>();
    double curve_y = unwrap(ad_primal(y_output)).as<double>();
    double curve_dx_dX = unwrap(ad_derivative(x_output, 0)).as<double>();
    double curve_dy_dX = unwrap(ad_derivative(y_output, 0)).as<double>();

    // Closed-form check: at u=0.5 the cubic Bezier weight on P_1 (the only pole X
    // feeds) is (1-u)^3 = 0.125, so curve_x should be exactly 1.625 and curve_dx_dX
    // exactly 0.125 - and since X never reaches P_1's Y coordinate, curve_y (a
    // fixed combination of P_1..P_4's literal y-coordinates) is exactly 0.375 and
    // curve_dy_dX exactly 0.
    std::cout << "curve point x(0.5) = " << curve_x << " (expected 1.625)\n"
                 "curve point y(0.5) = " << curve_y << " (expected 0.375)\n"
                 "d(curve point x(0.5))/dX = " << curve_dx_dX << " (expected 0.125)\n"
                 "d(curve point y(0.5))/dX = " << curve_dy_dX << " (expected 0, sanity check)"
              << std::endl;
}

int main() {

    std::cout << "Hello from cad_autodiff example!" << std::endl;

    std::cout << "\n[1/3] AD only (adtl.so): a grunk recipe built on ADOL-C's adouble type,\n"
                 "      no CAD geometry involved." << std::endl;
    write_autodiff_only_recipe();
    read_autodiff_only_recipe();

    std::cout << "\n[2/3] CAD only (geoml_plugin.so): the same kind of recipe, now built on\n"
                 "      geoml/OCCT geometry with plain doubles - no AD." << std::endl;
    write_cad_recipe(/*with_ad=*/false);
    read_cad_recipe();

    std::cout << "\n[3/3] CAD + AD (geoml_adolc_plugin.so): the same recipe as [2/3] again,\n"
                 "      verbatim, now built on adOCCT/geoml's feature/autodiff branch - X's\n"
                 "      derivative flows through the same curve construction." << std::endl;
    read_recipe_ad();

    std::cout << "\nDone." << std::endl;
    return 0;
}
