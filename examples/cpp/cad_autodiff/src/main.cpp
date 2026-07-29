// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
// SPDX-License-Identifier: MPL-2.0

#include <iostream>

#include <sol/types.hpp>
#include <grunk/grunk.hpp>

#include <dlfcn.h>
#include <stdexcept>

#include "adolc/adtl.h"

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
// plugins/geoml/geoml_plugin.cpp): unlike adtl.so (a compiled Lua module, loaded via
// load_compiled_plugin), it exposes a plain C++ entry point that registers
// types/functions directly against a grunk::state - grunk-dev/grunk#235's "C++
// plugin" kind. dlopen'd for the same reason as adtl.so: only its path needs to be
// known at build time (see CMakeLists.txt's geoml_plugin target).
using geoml_plugin_entry_point_t = void (*)(grunk::state&);

geoml_plugin_entry_point_t load_geoml_plugin_entry_point()
{
    // RTLD_LOCAL is safe here: every OCCT/geoml-typed sol2 template instantiation
    // (e.g. for Geom_BezierCurve) happens exclusively inside geoml_plugin.so -
    // construction and consumption (export_brep) both live in
    // geoml_registration.hpp - so there's no second copy on this side for the
    // dynamic linker to unify. This file only exchanges plain sol::object/bool
    // values with the plugin (see the file comment above).
    void* handle = dlopen(GEOML_PLUGIN_SO_PATH, RTLD_NOW | RTLD_LOCAL);
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

// Mechanically identical to load_geoml_plugin_entry_point/load_geoml_plugin above,
// just pointed at a different library and entry point symbol - see
// plugins/geoml_adolc/geoml_adolc_plugin.cpp and README.md's "Stage 3" section.
// plugins/geoml_adolc/ is a separate, standalone CMake project you build yourself
// (see that directory's CMakeLists.txt for why), so this throws unless
// GEOML_ADOLC_PLUGIN_SO_PATH (a CMake cache variable, ../../CMakeLists.txt) is
// pointed at a real build of it.
geoml_plugin_entry_point_t load_geoml_adolc_plugin_entry_point()
{
    void* handle = dlopen(GEOML_ADOLC_PLUGIN_SO_PATH, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        throw std::runtime_error(std::string("could not load geoml_adolc_plugin.so: ") + dlerror());
    }

    dlerror();
    void* sym = dlsym(handle, "grunk_geoml_adolc_plugin_entry_point");
    if (char const* err = dlerror(); err != nullptr) {
        throw std::runtime_error(std::string("could not find grunk_geoml_adolc_plugin_entry_point in geoml_adolc_plugin.so: ") + err);
    }

    return reinterpret_cast<geoml_plugin_entry_point_t>(sym);
}

void load_geoml_adolc_plugin(grunk::state& grunk)
{
    auto entry_point = load_geoml_adolc_plugin_entry_point();
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

// Stages 2 and 3 share this recipe verbatim - with_ad only changes which plugin
// loads, never the recipe text (the whole point, see the file comment above).
//
// Just one bezier curve, not a full CAD model - a dev artifact for exercising the
// plugin swap. X is a named feature (not a literal) so there's a single input for
// stage 3 to seed a derivative from - see read_recipe_ad below.
//
// point_at_half/x_at_half/y_at_half sample the curve at u=0.5 (Geom_BezierCurve.Value,
// then gp_Pnt's X()/Y() accessors - both genuine geoml_registration.hpp primitives,
// not example-specific) and x_at_half/y_at_half are declared as the recipe's named
// outputs (grunk-dev/grunk#266's `outputs:` block). Both work under either plugin,
// so this is safe to bake into the shared recipe text; only the AD-only derivative
// extraction stays confined to stage 3's read_recipe_ad, appended after read (via
// its own "ad" Lua module) rather than written here.
//
// Geom_BezierCurve.Value(curve, u), not curve:Value(u): calling a registered member
// function on a value flowing through the recipe's tracked/decorated environment
// needs this qualified "Type.method(instance, args...)" form (matching grunk's own
// test suite, e.g. `MyScalar.pow(c, 2)`, not `c:pow(2)`) - the decorated environment
// mirrors *functions* looked up by qualified name, not arbitrary methods on
// arbitrary tracked values, so Lua's `instance:method()` colon-call sugar doesn't
// reach it directly.
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

        P_1 = gp_Pnt.new(X, 0., 0.)
        P_2 = gp_Pnt.new(1., 2., 0.)
        P_3 = gp_Pnt.new(2., -1., 0.)
        P_4 = gp_Pnt.new(3., 0., 0.)

        poles = gp_Pnt.as_vec(P_1, P_2, P_3, P_4)
        curve = bezier_curve(poles)

        point_at_half = Geom_BezierCurve.Value(curve, 0.5)
        x_at_half = gp_Pnt.X(point_at_half)
        y_at_half = gp_Pnt.Y(point_at_half)
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
    recipe.eval(R"(exported = export_brep(curve, "bezier_curve.brep"))");

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
// Seeding X's derivative direction can't happen inside the recipe's own steps: X's
// value comes from the YAML (a plain number) and write_cad_recipe's own
// `P_1 = gp_Pnt.new(X, 0., 0.)` step already runs (lazily, but wired into the DAG)
// against that same X node before any step appended here would run. Re-assigning
// the Lua variable X wouldn't reach it either - P_1 depends on the specific
// DynamicFeature object already in the environment, not on whatever the name "X"
// happens to point to afterwards. So this seeds X's *existing* node in place via
// DynamicFeature::set_value (grunk/dynamic/DynamicFeature.hpp), which invalidates
// P_1/curve exactly like changing any other feature's value would.
void read_recipe_ad()
{
    auto grunk = grunk::state();
    load_geoml_adolc_plugin(grunk);

    auto recipe = grunk.read("bezier_curve.grr.yml");

    // "ad" composes genuine, general-purpose Standard_Real operations
    // (geoml_registration.hpp: construction, getValue, getADValue, setADValue)
    // into the seed/primal/derivative vocabulary this example needs. It's a Lua
    // module script inserted directly into the recipe (Recipe::insert_module_script,
    // the same mechanism stage 1 uses for "me" - write_autodiff_only_recipe), not a
    // plugin-registered function: the plugin only knows about OCCT/geoml/ADOL-C
    // itself, not this recipe's particular use of it.
    recipe.insert_module_script("ad", R"(
        function seed(x, value)
            ret = Standard_Real.new(x)
            Standard_Real.setADValue(ret, 0, value)
            return ret
        end

        function primal(x)
            return Standard_Real.getValue(x)
        end

        function derivative(x, direction)
            return Standard_Real.getADValue(x, direction)
        end
    )");

    // Retrieve X, the recipe's independent parameter, and seed its forward AD
    // direction before anything downstream is queried - otherwise every
    // derivative pulled out below is trivially zero. A seed of 1. reads a plain
    // d(.)/dX derivative back below; any other value would scale it linearly,
    // same as ADOL-C's setADValue itself.
    grunk::DynamicFeature X = recipe.get_feature("X");
    double x_val = X.value().as<double>();
    sol::table ad = recipe.get<sol::table>("ad");
    sol::protected_function ad_seed = ad["seed"];
    // ad.seed calls Standard_Real.new/setADValue, both genuinely tracked
    // functions (geoml_registration.hpp), so the result is itself a lazy
    // DynamicFeature, not the Standard_Adouble directly - .value() forces that
    // evaluation so X gets the resolved value (with its derivative direction
    // set), not a Feature-wrapping-a-Feature. X.set_value writes it onto X's own
    // node in place - P_1/curve already depend on that specific node from
    // grunk.read() above, so this is what actually invalidates and reseeds them,
    // unlike reassigning the Lua variable X (which wouldn't reach that
    // dependency edge).
    sol::object x_seeded = ad_seed(x_val, 1.);
    if (x_seeded.is<grunk::DynamicFeature>()) {
        x_seeded = x_seeded.as<grunk::DynamicFeature>().value();
    }
    X.set_value(x_seeded);

    // x_at_half/y_at_half (write_cad_recipe) are already Standard_Real-valued
    // once this recipe is re-evaluated under the AD plugin - ad.primal/
    // ad.derivative read the seeded value and direction-0 derivative straight
    // off them, no second Geom_BezierCurve.Value call needed.
    recipe.eval(R"(
        curve_x_primal = ad.primal(x_at_half)
        curve_y_primal = ad.primal(y_at_half)
        curve_dx_dX = ad.derivative(x_at_half, 0)
        curve_dy_dX = ad.derivative(y_at_half, 0)
    )");

    double curve_x = recipe.get_feature("curve_x_primal").value().as<double>();
    double curve_y = recipe.get_feature("curve_y_primal").value().as<double>();
    double curve_dx_dX = recipe.get_feature("curve_dx_dX").value().as<double>();
    double curve_dy_dX = recipe.get_feature("curve_dy_dX").value().as<double>();

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
