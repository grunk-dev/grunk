// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// Stage 3: the AD-enabled counterpart to ../geoml/geoml_plugin.cpp - same
// register_geoml (../geoml_registration.hpp), unmodified, just linked against a
// different geoml/OCCT install (adOCCT + geoml's feature/autodiff branch, see
// ../../README.md's "Stage 3" section and this directory's CMakeLists.txt). See that
// header's comment for why sharing it, instead of reimplementing, is the point.

#include "../geoml_registration.hpp"

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    // Same name as geoml_plugin.cpp's - see geoml_registration.hpp's file comment for
    // why. Version reflects the geoml branch this is built against (see README.md's
    // "Stage 3" section), not a release tag.
    return grunk::PluginInfo{"geoml", "feature/autodiff"};
}

GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& grunk, grunk::PluginInfo const& info)
{
    auto ns = grunk.begin_plugin(info);
    register_geoml(grunk, ns, info);
}
