// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

// A native grunk plugin fixture whose grunk_plugin_register deliberately throws after
// calling begin_plugin (which records the plugin's identity immediately - see
// state::begin_plugin) and registering one type, but before finishing registration -
// the shape a plugin with a bad sol2 registration call (e.g. a malformed usertype)
// would have. Used by plugin_loader.cpp to verify grunk::plugin::load_native rolls
// both the plugin's identity out of state::plugins() *and* the namespace contents it
// managed to register (e.g. RegisteredBeforeFailure below) back out, rather than
// reporting a half-registered plugin as loaded or leaving its partial registrations
// reachable.

#include <grunk/dynamic.hpp>
#include <grunk/plugin/loader.hpp>

#include <stdexcept>

namespace {

struct RegisteredBeforeFailure
{
    RegisteredBeforeFailure() = default;
};

void register_failing_fixture(grunk::state& state, grunk::PluginInfo const& info)
{
    auto ns = state.begin_plugin(info);
    state.register_type<RegisteredBeforeFailure>("RegisteredBeforeFailure", ns, info.name);
    throw std::runtime_error("deliberate failure for PluginLoader.load_native_rolls_back_failed_registration");
}

} // anonymous namespace

GRUNK_PLUGIN_EXPORT grunk::PluginInfo grunk_plugin_info()
{
    return grunk::PluginInfo{"failing_fixture", "1.0.0"};
}

GRUNK_PLUGIN_REGISTER(register_failing_fixture)
