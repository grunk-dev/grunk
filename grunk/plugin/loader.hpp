// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/state.hpp"

#include <filesystem>

namespace grunk::plugin {

/**
 * @brief The fixed, name-independent ABI a native grunk plugin (a plain C++ plugin,
 * or a compiled-Lua/SWIG shim - see load_native) exports, so a generic loader can
 * dlsym both symbols without already knowing the plugin's name:
 *
 * @code
 * extern "C" grunk::PluginInfo grunk_plugin_info();
 * extern "C" void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info);
 * @endcode
 *
 * `grunk_plugin_info` reports the plugin's identity; `grunk_plugin_register` is
 * handed that same PluginInfo back (rather than needing its own hardcoded copy) and
 * does whatever registration its kind requires:
 *
 * @code
 * // pure C++ plugin
 * extern "C" void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info) {
 *     auto ns = state.begin_plugin(info);
 *     state.register_type<gp_Pnt>("gp_Pnt", ns) ... ;
 * }
 *
 * // compiled-Lua (SWIG) shim
 * extern "C" void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info) {
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
 * @brief load_native dlopen's a shared library at @p path and invokes its
 * `grunk_plugin_info`/`grunk_plugin_register` entry points against @p state, per the
 * fixed native ABI documented above.
 *
 * This generalizes the dlopen/dlsym dance examples/cpp/cad_autodiff's src/main.cpp
 * currently hand-rolls once per plugin, with a bespoke symbol name and no shared
 * identity/bookkeeping (see load_geoml_plugin_entry_point there): dlopen(RTLD_NOW |
 * RTLD_LOCAL), dlsym both fixed symbols, call `grunk_plugin_info()` then
 * `grunk_plugin_register(state, info)`. What the plugin's own `grunk_plugin_register`
 * does with that call is entirely up to the plugin - this loader never needs to know
 * whether it is a plain C++ plugin or a compiled-Lua/SWIG shim.
 *
 * The loaded library handle is intentionally never dlclose'd: @p state - and the
 * Lua/sol2 references it holds into the plugin's registered functions/types - may
 * outlive this call for the remainder of the program, so unloading the library
 * underneath it would be unsafe.
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
