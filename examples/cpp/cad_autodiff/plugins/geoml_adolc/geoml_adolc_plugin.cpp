// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
// SPDX-FileCopyrightText: 2026 Mladen Banocic <mladen.banovic@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// Stage 3 scaffold: the AD-enabled counterpart to ../geoml/geoml_plugin.cpp - same
// register_geoml (../geoml_registration.hpp), unmodified, just linked against a
// different geoml/OCCT install. See that header's comment for why sharing it,
// instead of reimplementing, is the point.
//
// TODO(stage3): not yet built. Needs an adOCCT install, geoml's feature/autodiff
// branch built against it, and reconciling geoml's OCCT 7.6.2 pin with adOCCT's OCCT
// V7_9_0 base. See ../../README.md's "Stage 3" section and this directory's
// CMakeLists.txt.

#include "../geoml_registration.hpp"

extern "C" void grunk_geoml_adolc_plugin_entry_point(grunk::state& grunk)
{
    register_geoml(grunk);
}
