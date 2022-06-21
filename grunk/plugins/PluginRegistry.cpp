#include "PluginRegistry.h"
#include "StdPlugin.h"
#include <boost/dll/import.hpp>
#include <functional>
#include <iostream>

namespace grunk {

PluginRegistry::PluginRegistry(const std::filesystem::path& plugins_dir)
    : plugins_directory(plugins_dir)
{
    // insert "standard library plugin"
    auto s = std::make_unique<StdPlugin>();
    s->init();

    // load all other plugins
    load_all();
}

void PluginRegistry::load_all() {
    namespace fs = std::filesystem;

    // Searching a folder for files with '.so' or '.dll' extension
    fs::recursive_directory_iterator endit;
    for (fs::recursive_directory_iterator it(plugins_directory); it != endit; ++it) {

        if (!fs::is_regular_file(*it)) {
            continue;
        }

        auto ext = it->path().extension().string();
        if ( ext == ".dll" || ext == ".so" )  {

            boost::dll::fs::error_code error;
            boost::dll::shared_library lib(it->path(), error);
            if (error) {
                continue;
            }

            if (lib.has("create_grunk_plugin")) {
                insert_plugin(std::move(lib));
            }
        }
    }

}

void PluginRegistry::insert_plugin(BOOST_RV_REF(boost::dll::shared_library) lib)
{
    using PluginFactoryFunc = std::unique_ptr<IPlugin>();
    auto creator = boost::dll::import_alias<PluginFactoryFunc>(
        lib,
        "create_grunk_plugin"
    );
    std::unique_ptr<IPlugin> plugin = creator();
    std::string name = plugin->name();

    // register the plugin, if it hasn't been registered yet
    if (auto it = loaded_plugins.find(name); it == loaded_plugins.end()) {
        plugin->init();
        loaded_plugins[name] = std::move(lib);
    }
}

void PluginRegistry::print_plugins() const {
    for (const auto& [name, plugin] : loaded_plugins) {
            std::cout << '[' << name << "]\n";
    }
}

std::size_t PluginRegistry::count() const {
    return loaded_plugins.size();
}

PluginRegistry::~PluginRegistry()
{
    // TODO: This is a bit ugly: We need to clear types that depend on the loaded
    // plugins to prevent a segmentation fault at program end.

    // Cleaner would be: Invalidate all types of a plugin once the plugin is unloaded. 
    // Or: No static type registry, if types depend on loaded plugins.

    // Or: Figure out why plugins have to stay alive at all and check if this can 
    // be circumvented somehow.
    Reflect::GetTypeRegistry().clear();
}

} //namespace grunk