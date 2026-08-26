// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#include "grunk/plugin/loader.hpp"

#include <dlfcn.h>
#include <stdexcept>

namespace grunk::plugin {

namespace {

void* dlsym_checked(void* handle, char const* symbol, std::string const& path)
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

} // anonymous namespace

PluginInfo load_native(state& state, std::filesystem::path const& path)
{
    std::string path_str = path.string();

    // RTLD_LOCAL keeps a plugin's own symbols out of the global symbol table - safe as
    // long as it doesn't rely on some *other* dlopen'd library resolving symbols
    // against it. RTLD_NOW surfaces any unresolved symbol immediately, rather than
    // deferring the failure to whichever call happens to hit it first.
    void* handle = dlopen(path_str.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        throw std::runtime_error("grunk::plugin::load_native: could not load \"" + path_str + "\": " + dlerror());
    }

    auto info_fn = reinterpret_cast<info_fn_t>(dlsym_checked(handle, "grunk_plugin_info", path_str));
    auto register_fn = reinterpret_cast<register_fn_t>(dlsym_checked(handle, "grunk_plugin_register", path_str));

    PluginInfo info = info_fn();
    register_fn(state, info);
    return info;
}

sol::table load_script(state& state, PluginInfo const& info, std::filesystem::path const& path)
{
    return state.load_lua_plugin_file(info, path.string());
}

} // namespace grunk::plugin
