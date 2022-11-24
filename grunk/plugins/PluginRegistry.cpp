#include "PluginRegistry.h"
#include "StdPlugin.h"
#include <boost/dll/import.hpp>
#include <functional>
#include <iostream>

namespace grunk {

PluginRegistry::PluginRegistry()
{
    // insert "standard library plugin"
    auto s = std::make_unique<StdPlugin>();
    s->init();
}

void PluginRegistry::prepend_path(std::string const& dir)
{
    path.insert(path.begin(), dir);
}

void PluginRegistry::load_all() {

    namespace fs = std::filesystem;

    // Searching a folder for files with '.so' or '.dll' extension
    for(auto const& plugins_directory : path) {
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

}

void PluginRegistry::unload_all()
{
    loaded_plugins.clear();
}

void PluginRegistry::insert_plugin(BOOST_RV_REF(boost::dll::shared_library) lib)
{
    auto creator = boost::dll::import_alias<std::unique_ptr<IPlugin>(void)>(
        lib,
        "create_grunk_plugin"
    );

    Entry e;
    e.plugin = creator();
    e.library = std::move(lib);

    // insert the plugin
    std::string name = e.plugin->name();
    auto [it, inserted] = loaded_plugins.try_emplace(name, std::move(e));

    // register the plugin, if it hasn't been registered yet
    if (inserted) {
        it->second.plugin->init();
    }
}

void PluginRegistry::print_plugins() const {
    for (const auto& [name, entry] : loaded_plugins) {
            std::cout << name << ": " << entry.plugin->version() << "\n";
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
    reflect::get_type_registry().clear();
}

PluginRegistry& get_plugin_registry()
{
    static auto registry = PluginRegistry();
    return registry;
}

} //namespace grunk