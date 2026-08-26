// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// A minimal native grunk plugin fixture, exporting the fixed grunk_plugin_info/
// grunk_plugin_register ABI grunk::plugin::load_native expects (see
// grunk/plugin/loader.hpp). Built as its own shared library (see this directory's
// CMakeLists.txt) and dlopen'd by plugin_loader.cpp - the one test that needs a real
// .so on disk rather than an in-process function pointer (contrast with
// dynamic_plugin.cpp's luaopen_testplugin, which stands in for a compiled-Lua module
// without ever leaving the test binary).
//
// Registers exactly the way a plain C++ plugin is meant to (see state::begin_plugin):
// a namespace table, a type, and a function against it - the same shape
// examples/cpp/cad_autodiff's geoml_plugin.cpp uses, just namespaced.

#include <grunk/dynamic.hpp>
#include <grunk/plugin/loader.hpp>

namespace {

struct FixtureType
{
    FixtureType() = default;
    FixtureType(double x_) : x(x_) {}
    double get_x() const { return x; }
    double x{0.};
};

} // anonymous namespace

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"native_fixture", "1.0.0"};
}

GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info)
{
    auto ns = state.begin_plugin(info);
    state.register_type<FixtureType>("FixtureType", ns, info.name)
        .add_constructors([](double x) { return FixtureType(x); })
        .add_member_function("get_x", &FixtureType::get_x);
    state.register_function(
        "twice",
        [](FixtureType const& f) { return 2. * f.get_x(); },
        {},
        ns,
        info.name
    );
}
