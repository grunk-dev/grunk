// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/state.hpp"

#include <filesystem>

/**
 * @brief Marks a native plugin's two entry-point definitions (grunk_plugin_info,
 * grunk_plugin_register - see below) for cross-platform symbol export.
 *
 * On Linux/macOS, `extern "C"` alone is enough - GCC/Clang export a shared library's
 * symbols by default. On Windows, a DLL exports nothing by default; a symbol needs
 * either `__declspec(dllexport)` on its definition (what this macro adds) or the
 * whole plugin project needs `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON` (what grunk's own
 * top-level CMakeLists.txt sets for itself, but which a separate, third-party
 * plugin project - e.g. examples/cpp/cad_autodiff's geoml_plugin - would need to set
 * too, not inherit). Prefixing both entry points with this macro instead means a
 * plugin's source is portable across all three platforms without either project
 * needing to know which.
 *
 * @ingroup plugin
 */
#if defined(_WIN32)
#define GRUNK_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define GRUNK_PLUGIN_EXPORT extern "C"
#endif

namespace grunk::plugin {

/**
 * @brief The fixed, name-independent ABI a native grunk plugin (a plain C++ plugin,
 * or a compiled-Lua/SWIG shim - see load_native) exports, so a generic loader can
 * look up both symbols without already knowing the plugin's name:
 *
 * @code
 * GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info();
 * GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info);
 * @endcode
 *
 * `grunk_plugin_info` reports the plugin's identity; `grunk_plugin_register` is
 * handed that same PluginInfo back (rather than needing its own hardcoded copy) and
 * does whatever registration its kind requires:
 *
 * @code
 * // pure C++ plugin
 * GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info) {
 *     auto ns = state.begin_plugin(info);
 *     state.register_type<gp_Pnt>("gp_Pnt", ns) ... ;
 * }
 *
 * // compiled-Lua (SWIG) shim
 * GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info) {
 *     sol::table ns = state.load_compiled_plugin(info, luaopen_adtl);
 *     state.register_external_type(info.name + ".adouble", ns["adouble"], ns);
 * }
 * @endcode
 *
 * A pre-built namespace table is deliberately not part of this ABI: load_compiled_plugin
 * must mint its own table (the module's own table from state::load_module), so a table
 * the loader pre-created via state::begin_plugin would be redundant/wrong for that
 * path. Passing `(state&, info)` lets each kind call whichever of
 * state::begin_plugin/state::load_compiled_plugin actually fits its own shape.
 *
 * @ingroup plugin
 */
using info_fn_t = PluginInfo (*)();
using register_fn_t = void (*)(state&, PluginInfo const&);

/**
 * @brief load_native loads a shared library at @p path (a `.so`/`.dylib` on
 * Linux/macOS, a `.dll` on Windows) and invokes its `grunk_plugin_info`/
 * `grunk_plugin_register` entry points against @p state, per the fixed native ABI
 * documented above.
 *
 * This generalizes the dlopen/dlsym-style dance examples/cpp/cad_autodiff's
 * src/main.cpp used to hand-roll once per plugin, with a bespoke symbol name and no
 * shared identity/bookkeeping: load the library, look up both fixed symbols, call
 * `grunk_plugin_info()` then `grunk_plugin_register(state, info)`. What the plugin's
 * own `grunk_plugin_register` does with that call is entirely up to the plugin - this
 * loader never needs to know whether it is a plain C++ plugin or a compiled-Lua/SWIG
 * shim, or which platform it's running on (see loader.cpp for the
 * dlopen/dlsym vs. LoadLibrary/GetProcAddress split).
 *
 * The loaded library handle is intentionally never released (dlclose/FreeLibrary):
 * @p state - and the Lua/sol2 references it holds into the plugin's registered
 * functions/types - may outlive this call for the remainder of the program, so
 * unloading the library underneath it would be unsafe.
 *
 * @param state the state to register the plugin's types/functions into
 * @param path path to the plugin's shared library
 * @return the PluginInfo the plugin reported via `grunk_plugin_info()`
 */
PluginInfo load_native(state& state, std::filesystem::path const& path);

/**
 * @brief load_script starts a "pure Lua" plugin from a file on disk - a thin
 * convenience wrapper over state::load_lua_plugin_file, provided here so all three
 * plugin kinds (native C++, compiled-Lua/SWIG, pure Lua) have an entry point in one
 * place, grunk::plugin.
 *
 * @param state the state to register the plugin's namespace into
 * @param info the plugin's name and version - a bare .lua file carries no identity
 *             of its own, unlike a native plugin's `grunk_plugin_info` (see
 *             state::load_lua_plugin_file)
 * @param path path to the plugin's Lua source file
 * @return the plugin's namespace table
 */
sol::table load_script(state& state, PluginInfo const& info, std::filesystem::path const& path);

} // namespace grunk::plugin
