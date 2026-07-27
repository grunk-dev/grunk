// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// Stage 2 plugin: CAD via geoml/OCCT, no AD. Built as its own shared object (see
// CMakeLists.txt's geoml_plugin target), dlopen'd by src/main.cpp's
// load_geoml_plugin - see that function's comment for why. Registration itself
// (register_geoml) lives in ../geoml_registration.hpp, shared verbatim with
// geoml_adolc/geoml_adolc_plugin.cpp - see that header's comment for why.

#include "../geoml_registration.hpp"

// Symbol main.cpp's load_geoml_plugin_entry_point dlsym's. extern "C" for stable,
// unmangled linkage - the same requirement a Lua module's luaopen_* entry point has.
extern "C" void grunk_geoml_plugin_entry_point(grunk::state& grunk)
{
    register_geoml(grunk);
}
