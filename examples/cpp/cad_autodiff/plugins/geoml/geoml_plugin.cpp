// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// Stage 2 plugin: CAD via geoml/OCCT, no AD. Built as its own shared object (see
// CMakeLists.txt's geoml_plugin target), loaded via grunk::plugin::load_native
// (src/main.cpp's load_geoml_plugin). Registration itself (register_geoml) lives in
// ../geoml_registration.hpp, shared verbatim with geoml_adolc/geoml_adolc_plugin.cpp -
// see that header's comment for why.

#include "../geoml_registration.hpp"

// grunk_plugin_info/grunk_plugin_register: the fixed ABI grunk::plugin::load_native
// dlsym's, extern "C" for stable, unmangled linkage - see grunk/plugin/loader.hpp.
extern "C" grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"geoml", "main"}; // tracks ../../CMakeLists.txt's FetchContent GIT_TAG
}

extern "C" void grunk_plugin_register(grunk::state& grunk, grunk::PluginInfo const& info)
{
    auto ns = grunk.begin_plugin(info);
    register_geoml(grunk, ns, info);
}
