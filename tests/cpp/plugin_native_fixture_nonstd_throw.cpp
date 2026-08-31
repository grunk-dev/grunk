// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// A native grunk plugin fixture whose registration function throws a value that is
// not derived from std::exception (a plain int) - the shape a plugin author's
// mistake, or a third-party library's own non-standard exception type, could take.
// GRUNK_PLUGIN_REGISTER's generated grunk_plugin_register must still catch this (its
// own `catch (...)` branch) and report it as a plain error message instead of letting
// it propagate - an uncaught non-std::exception unwinding out of grunk_plugin_register
// would hit the same cross-dlopen-boundary hazard register_fn_t's own doc comment
// describes for std::exception, just with no way to even read a `.what()` message
// first. Used by plugin_loader.cpp's
// PluginLoader.load_native_reports_non_std_exception_cleanly.

#include <grunk/dynamic.hpp>
#include <grunk/plugin/loader.hpp>

namespace {

void register_nonstd_throw_fixture(grunk::state&, grunk::PluginInfo const&)
{
    throw 42;
}

} // anonymous namespace

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"nonstd_throw_fixture", "1.0.0"};
}

GRUNK_PLUGIN_REGISTER(register_nonstd_throw_fixture)
