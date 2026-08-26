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
    try {
        info_fn = reinterpret_cast<info_fn_t>(find_symbol_checked(handle, "grunk_plugin_info", path_str));
        register_fn = reinterpret_cast<register_fn_t>(find_symbol_checked(handle, "grunk_plugin_register", path_str));
    } catch (...) {
        close_library(handle);
        throw;
    }

    PluginInfo info = info_fn();
    try {
        register_fn(state, info);
    } catch (std::exception const& e) {
        // Registration may have partially completed (e.g. begin_plugin already
        // recorded the plugin's identity before the caller's own register_type/
        // register_function calls threw) - forget it so a half-registered plugin is
        // never reported as loaded, and wrap the error with which plugin/library it
        // came from, since the original exception (typically a bare sol2/Lua error)
        // has no idea it was thrown from inside a plugin's registration.
        state.forget_plugin(info.name);
        throw std::runtime_error(
            "grunk::plugin::load_native: plugin \"" + info.name + "\" (from \"" + path_str +
            "\") failed during registration: " + e.what()
        );
    } catch (...) {
        state.forget_plugin(info.name);
        throw;
    }
    return info;
}

sol::table load_script(state& state, PluginInfo const& info, std::filesystem::path const& path)
{
    return state.load_lua_plugin_file(info, path.string());
}

} // namespace grunk::plugin
