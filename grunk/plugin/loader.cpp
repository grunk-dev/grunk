// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include "grunk/plugin/loader.hpp"

#include <stdexcept>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace grunk::plugin {

namespace {

#if defined(_WIN32)

using native_handle = HMODULE;

std::string last_error_message()
{
    DWORD err = GetLastError();
    if (err == 0) {
        return "unknown error";
    }

    LPSTR buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer), 0, nullptr
    );
    // FormatMessageA can fail (return 0, leave buffer null) for an error code with no
    // message-table entry - guard against constructing a std::string from a null
    // buffer and calling LocalFree on it in that case.
    if (size == 0 || buffer == nullptr) {
        return "unknown error (code " + std::to_string(err) + ")";
    }
    std::string message(buffer, size);
    LocalFree(buffer);
    // FormatMessageA's system messages conventionally end with "\r\n" - trim it so a
    // thrown exception's message doesn't contain an embedded line break.
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }
    return message;
}

native_handle open_library(std::filesystem::path const& path)
{
    // LoadLibraryW (not the narrow LoadLibraryA) so a non-ASCII path round-trips
    // correctly - std::filesystem::path::wstring() already handles that conversion.
    return LoadLibraryW(path.wstring().c_str());
}

void close_library(native_handle handle)
{
    FreeLibrary(handle);
}

void* find_symbol_checked(native_handle handle, char const* symbol, std::string const& path)
{
    void* sym = reinterpret_cast<void*>(GetProcAddress(handle, symbol));
    if (!sym) {
        throw std::runtime_error(
            "grunk::plugin::load_native: could not find \"" + std::string(symbol) +
            "\" in \"" + path + "\": " + last_error_message()
        );
    }
    return sym;
}

#else

using native_handle = void*;

std::string last_error_message()
{
    char const* err = dlerror();
    return err ? err : "unknown error";
}

native_handle open_library(std::filesystem::path const& path)
{
    // RTLD_LOCAL keeps a plugin's own symbols out of the global symbol table - safe as
    // long as it doesn't rely on some *other* dlopen'd library resolving symbols
    // against it. RTLD_NOW surfaces any unresolved symbol immediately, rather than
    // deferring the failure to whichever call happens to hit it first. Windows has no
    // equivalent switch: LoadLibrary always resolves eagerly against each DLL's own
    // explicit export table, never a shared global namespace.
    return dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
}

void close_library(native_handle handle)
{
    dlclose(handle);
}

void* find_symbol_checked(native_handle handle, char const* symbol, std::string const& path)
{
    dlerror(); // clear any prior error, per dlsym's own documented idiom for telling a
               // valid NULL result apart from a real lookup failure
    void* sym = dlsym(handle, symbol);
    if (char const* err = dlerror(); err != nullptr) {
        throw std::runtime_error(
            "grunk::plugin::load_native: could not find \"" + std::string(symbol) +
            "\" in \"" + path + "\": " + err
        );
    }
    return sym;
}

#endif

} // anonymous namespace

PluginInfo load_native(state& state, std::filesystem::path const& path)
{
    std::string path_str = path.string();

    native_handle handle = open_library(path);
    if (!handle) {
        throw std::runtime_error("grunk::plugin::load_native: could not load \"" + path_str + "\": " + last_error_message());
    }

    info_fn_t info_fn;
    register_fn_t register_fn;
    PluginInfo info;
    try {
        info_fn = reinterpret_cast<info_fn_t>(find_symbol_checked(handle, "grunk_plugin_info", path_str));
        register_fn = reinterpret_cast<register_fn_t>(find_symbol_checked(handle, "grunk_plugin_register", path_str));
        // grunk_plugin_info() itself may throw (e.g. building its version string
        // fails) - handled by this same try/catch so the handle is closed here too,
        // not just on a symbol-lookup failure. Nothing has touched `state` yet at
        // this point, so unloading the library here is always safe.
        info = info_fn();
    } catch (...) {
        close_library(handle);
        throw;
    }

    // If anything at all - a plugin, or a plain, non-plugin module/symbol - already
    // occupies this name, this call cannot have registered anything new (state's
    // begin_plugin/load_compiled_plugin/load_lua_plugin_script all reject that
    // immediately via assert_plugin_name_free) - the failure belongs entirely to
    // *this* call, not to whatever pre-existing thing occupies the name, so
    // forget_plugin must not run below and erase it. name_occupied checks both cases
    // (it used to only check state.plugins() here, missing the "occupied by a plain
    // module" collision, which could make forget_plugin wipe an unrelated,
    // pre-existing module out from under a host application).
    bool const already_occupied = state.name_occupied(info.name);

    // Registration may have partially completed (e.g. begin_plugin already recorded
    // the plugin's identity before the caller's own register_type/register_function
    // calls failed) - forget it so a half-registered plugin is never reported as
    // loaded. Unlike the symbol-lookup failure above, the library handle is
    // deliberately never closed here: once registration has started, `state` (and the
    // Lua VM it owns) may hold live references into objects the plugin's own code
    // created - e.g. a usertype metatable with a sol2-installed finalizer - that
    // forget_plugin's bookkeeping drops grunk's own references to, but that Lua's own
    // garbage collector may not actually sweep until later. Unloading the library out
    // from under such a still-pending reference would let that later collection jump
    // into now-unmapped memory. This mirrors the exact same reasoning load_native
    // already applies to the *success* path below (the handle is never released
    // there either) - a failed/partial registration can leave the same kind of
    // live references, so the same invariant has to hold, at the cost of leaking the
    // (unused) library mapping on a failed load.
    auto rollback = [&] {
        if (!already_occupied) {
            state.forget_plugin(info.name);
        }
    };
    auto fail = [&](std::string const& message) -> std::runtime_error {
        rollback();
        return std::runtime_error(
            "grunk::plugin::load_native: plugin \"" + info.name + "\" (from \"" + path_str +
            "\") failed during registration: " + message
        );
    };

    // register_fn (grunk_plugin_register, generated by GRUNK_PLUGIN_REGISTER - see
    // loader.hpp's register_fn_t doc comment) reports failure by returning an error
    // message instead of throwing: a plugin author's own exception is already caught
    // and converted to this on the plugin's own side of the dlopen boundary, so no
    // C++ exception needs to unwind across it here. The try/catch below is only a
    // defense-in-depth fallback for a plugin that doesn't use the macro (e.g. a
    // hand-rolled, not-yet-migrated entry point) and still throws directly - safe only
    // if that plugin happens to share this process's exact compiler/stdlib ABI.
    char const* error = nullptr;
    try {
        error = register_fn(state, info);
    } catch (std::exception const& e) {
        throw fail(e.what());
    } catch (...) {
        rollback();
        throw;
    }

    if (error) {
        throw fail(error);
    }

    return info;
}

sol::table load_script(state& state, PluginInfo const& info, std::filesystem::path const& path)
{
    return state.load_lua_plugin_file(info, path);
}

} // namespace grunk::plugin
