// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "grunk/dynamic/state.hpp"

#include <exception>
#include <filesystem>
#include <string>

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
 * GRUNK_PLUGIN_EXPORT char const* grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info);
 * @endcode
 *
 * `grunk_plugin_info` reports the plugin's identity. `grunk_plugin_register` is
 * handed that same PluginInfo back (rather than needing its own hardcoded copy), does
 * whatever registration its kind requires, and reports failure by *returning* a
 * pointer to a null-terminated error message (owned by the plugin, valid until the
 * next call into it - see GRUNK_PLUGIN_REGISTER below) rather than by throwing - a
 * plain C++ exception is not safe to unwind across the dlopen/dlsym boundary between
 * this library and the plugin's own shared object unless both were built with the
 * exact same compiler, C++ standard library, and grunk/sol2/Lua versions; when that
 * assumption doesn't hold (a real risk once plugins are built as separate projects,
 * as every example plugin already is), an uncaught exception crossing the boundary is
 * undefined behavior - typically std::terminate(), which kills the whole host process
 * before load_native's own rollback (state::forget_plugin) ever gets to run.
 *
 * Do not implement `grunk_plugin_register` by hand - use the GRUNK_PLUGIN_REGISTER
 * macro, which generates the correct, exception-safe entry point around an ordinary
 * function/lambda that itself still just returns void and is free to throw: the
 * macro's own try/catch runs entirely on the plugin's side of the boundary (an
 * ordinary, in-process call, not a dlopen/dlsym one), so no exception ever needs to
 * unwind across it - only the resulting `char const*` does, which is always ABI-safe.
 *
 * @code
 * // pure C++ plugin
 * namespace {
 * void register_my_plugin(grunk::state& state, grunk::PluginInfo const& info) {
 *     auto ns = state.begin_plugin(info);
 *     state.register_type<gp_Pnt>("gp_Pnt", ns) ... ;
 * }
 * }
 * GRUNK_PLUGIN_REGISTER(register_my_plugin)
 *
 * // compiled-Lua (SWIG) shim
 * namespace {
 * void register_adtl(grunk::state& state, grunk::PluginInfo const& info) {
 *     sol::table ns = state.load_compiled_plugin(info, luaopen_adtl);
 *     state.register_external_type(info.name + ".adouble", ns["adouble"], ns);
 * }
 * }
 * GRUNK_PLUGIN_REGISTER(register_adtl)
 * @endcode
 *
 * A pre-built namespace table is deliberately not part of this ABI: load_compiled_plugin
 * must mint its own table (the module's own table from state::load_module), so a table
 * the loader pre-created via state::begin_plugin would be redundant/wrong for that
 * path. Passing `(state&, info)` lets each kind call whichever of
 * state::begin_plugin/state::load_compiled_plugin actually fits its own shape.
 *
 * Even with GRUNK_PLUGIN_REGISTER, `state&`/`PluginInfo const&` themselves still cross
 * the boundary as real C++ objects (not an opaque/stable C ABI), so this only removes
 * the *exception-unwinding* half of the cross-toolchain hazard - a plugin still needs
 * to be built against the same grunk/sol2/Lua versions as the host, and with a
 * compiler/standard library ABI-compatible with it, or its calls into `state` are
 * themselves undefined behavior regardless of this macro.
 *
 * @ingroup plugin
 */
using info_fn_t = PluginInfo (*)();
using register_fn_t = char const* (*)(state&, PluginInfo const&);

/**
 * @brief Defines a plugin's `grunk_plugin_register` entry point around @p fn (a
 * `void(grunk::state&, grunk::PluginInfo const&)` function or lambda-convertible-to-
 * function-pointer that does the plugin's actual registration and is free to throw
 * any std::exception on failure, exactly like the pre-GRUNK_PLUGIN_REGISTER examples
 * in this header's own doc comment used to).
 *
 * The generated entry point catches on the plugin's own side of the dlopen/dlsym
 * boundary (never across it - see register_fn_t's doc comment for why that matters)
 * and reports failure as a `char const*` instead: nullptr on success, or a
 * null-terminated message on failure, valid until the next call into
 * `grunk_plugin_register` in this same shared library (it is stored in a
 * function-local static, not returned by value, to keep the ABI a single, trivially
 * copyable pointer).
 *
 * @param fn the plugin's actual registration function/lambda
 */
#define GRUNK_PLUGIN_REGISTER(fn) \
    GRUNK_PLUGIN_EXPORT char const* grunk_plugin_register(grunk::state& grunk_plugin_register_state, grunk::PluginInfo const& grunk_plugin_register_info) \
    { \
        static thread_local std::string grunk_plugin_register_error_message; \
        try { \
            (fn)(grunk_plugin_register_state, grunk_plugin_register_info); \
            return nullptr; \
        } catch (std::exception const& grunk_plugin_register_exc) { \
            grunk_plugin_register_error_message = grunk_plugin_register_exc.what(); \
            return grunk_plugin_register_error_message.c_str(); \
        } catch (...) { \
            grunk_plugin_register_error_message = "unknown exception (not derived from std::exception)"; \
            return grunk_plugin_register_error_message.c_str(); \
        } \
    }

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
