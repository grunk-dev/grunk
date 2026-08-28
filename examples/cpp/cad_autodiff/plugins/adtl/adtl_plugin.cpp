// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// Stage 1 plugin: a thin shim around adtl.so, a SWIG-generated Lua module wrapping
// ADOL-C's tapeless adouble type (see
// https://gitlab.dlr.de/dlr-sp/occt-differentiation/swig-adol-c, lua_wrapper branch).
// Built as its own shared object (see ../../CMakeLists.txt's adtl_plugin target),
// loaded via grunk::plugin::load_native (src/main.cpp's load_adolc_plugin) exactly
// like geoml_plugin.so - grunk-dev/grunk#235's "compiled Lua" plugin kind, made to
// look native to the loader by exporting the same fixed ABI a plain C++ plugin does.
//
// This is also where the SWIG-specific bridging work lives that a plain
// load_compiled_plugin call can't do on its own: adouble's instance methods aren't
// plain table entries (see register_external_type/external_type_proxy for why), and
// adouble.i's `%ignore operator<<` leaves it with no __tostring, which grunk needs to
// serialize an adouble-carrying Feature into recipe YAML at all. Concentrating this
// here - rather than in application code, which is where it lived before this plugin
// existed - is the whole point: an application loading adtl_plugin.so only ever sees
// PluginInfo + grunk::state, the same as any other plugin kind.

#include <grunk/grunk.hpp>
#include <sol/types.hpp>

#include <stdexcept>

// adtl.so's own SWIG-generated Lua entry point, following Lua's own luaopen_*
// convention. Linked directly (see ../../CMakeLists.txt) rather than dlopen'd by hand
// - grunk::plugin::load_native only ever dlopen's this shim itself, one level up.
extern "C" int luaopen_adtl(lua_State* L);

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"adtl", "2.7.2"}; // matches the wrapped ADOL-C release
}

namespace {

void register_adtl_plugin(grunk::state& grunk, grunk::PluginInfo const& info)
{
    // Load the compiled SWIG-Lua module as a grunk plugin: its own table becomes the
    // "adtl" namespace in original_env, its free functions (tan, exp, log, sqrt, pow,
    // ...) are made grunk-tracked, and its identity is recorded in grunk.plugins().
    auto adtl = grunk.load_compiled_plugin(info, luaopen_adtl);

    // No .add_member_function calls are needed: adouble's default constructor lets
    // register_external_type probe an instance and discover setADValue/getADValue/
    // getValue (and any other method actually used) lazily, the first time each is
    // looked up - SWIG-Lua gives no way to enumerate them up front. plugin_namespace's
    // register_external_type auto-prefixes "adouble" into "adtl.adouble" for us.
    sol::table adouble_static = adtl["adouble"];
    adtl.register_external_type("adouble", adouble_static);

    // adouble.i explicitly `%ignore`s operator<<, so the SWIG binding gives adouble
    // instances no __tostring - without one, grunk can't serialize an adouble held
    // directly by a Feature (e.g. one built via new_feature) into recipe YAML at all.
    // Bridge one here in ctor syntax, so Serializer's ctor_syntax_to_new_feature_syntax
    // can turn a written-out parameter back into a "new_feature" call on read-back,
    // exactly the convention grunk-registered types use (see grunk's own test suite,
    // where MyScalar's tostring returns "MyScalar.new(...)" for the same reason).
    sol::table adouble_type = grunk.get_type(info.name + ".adouble");
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
        [get_value, name = info.name](sol::object self) -> std::string {
            double value = get_value(self).get<double>();
            return name + ".adouble.new(" + grunk::to_string(value) + ")";
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
        // Every other key must fall through to SWIG's original dispatcher unchanged -
        // including its failure behavior: a bad key (e.g. a typo'd accessor) must
        // still raise SWIG's own "no such attribute" error here, not be silently
        // swallowed into nil, which would let the mistake surface later, elsewhere,
        // as a more confusing unrelated failure.
        sol::protected_function_result res = original_index(self, key);
        if (!res.valid()) {
            sol::error err = res;
            throw std::runtime_error(err.what());
        }
        return res;
    });
}

} // anonymous namespace

GRUNK_PLUGIN_REGISTER(register_adtl_plugin)
