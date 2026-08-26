// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// A native grunk plugin fixture whose grunk_plugin_register deliberately throws after
// calling begin_plugin (which records the plugin's identity immediately - see
// state::begin_plugin) but before finishing registration - the shape a plugin with a
// bad sol2 registration call (e.g. a malformed usertype) would have. Used by
// plugin_loader.cpp to verify grunk::plugin::load_native rolls the plugin's identity
// back out of state::plugins() rather than reporting a half-registered plugin as loaded.

#include <grunk/dynamic.hpp>
#include <grunk/plugin/loader.hpp>

#include <stdexcept>

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"failing_fixture", "1.0.0"};
}

GRUNK_PLUGIN_EXPORT void grunk_plugin_register(grunk::state& state, grunk::PluginInfo const& info)
{
    state.begin_plugin(info);
    throw std::runtime_error("deliberate failure for PluginLoader.load_native_rolls_back_failed_registration");
}
