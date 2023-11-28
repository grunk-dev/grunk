#include <grunk/plugins/PluginRegistry.hpp>
#include <grunk/common/init.hpp>
#include <grunk/common/common_functions.hpp>
#include <boost/dll/import.hpp>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace grunk {

PluginRegistry::PluginRegistry()
{
    // make sure built-in types are registered
    grunk::init();
    populate_path_from_env();
}

void PluginRegistry::prepend_path(std::string const& dir)
{
    std::filesystem::path p(dir);
    if (std::filesystem::is_directory(p)) {
        path.push_front(p);
    }
}

void PluginRegistry::append_path(std::string const& dir)
{
    std::filesystem::path p(dir);
    if (std::filesystem::is_directory(p)) {
        path.push_back(p);
    }
}

void PluginRegistry::populate_path_from_env()
{
#if defined (__WIN32__)
    std::string delimiter = ";";
#else 
    std::string delimiter = ":";
#endif

    std::vector<std::string> env_vars = {"DYLD_LIBRARY_PATH", "PATH", "LD_LIBRARY_PATH"};
    
    for (auto const& var: env_vars) {
        if (const char* paths = std::getenv(var.c_str()); paths) {
            auto dirs = split(paths, delimiter);
            for (auto it = dirs.crbegin(); it != dirs.crend(); ++it )
            {
                if (!it->empty()) {
                    prepend_path(*it);
                }
            }
        }
    }
}

std::optional<std::filesystem::path> PluginRegistry::find_shared_lib(std::string_view name) const
{
    namespace fs = std::filesystem;

    // Searching a folder for files with '.so' or '.dll' extension
    for(auto const& plugins_directory : path) {
        fs::directory_iterator endit;
        for (fs::directory_iterator it(plugins_directory); it != endit; ++it) {
            if (!fs::is_regular_file(*it)) {
                continue;
            }
            
            auto ext = it->path().extension().string();
            if ( ext == ".dll" || ext == ".so" )  {
                auto stem = it->path().stem().string();
                if (stem.find(name) != std::string::npos){
                    return it->path();
                }
            }
        }
    }
    return std::nullopt;
}

void PluginRegistry::load(std::string const& name) 
{
    if(auto shared_lib = find_shared_lib(name); shared_lib) {
        boost::dll::fs::error_code error;
        try {
            boost::dll::shared_library lib(*shared_lib, error);
            if (error) {
                throw std::runtime_error(
                    std::string("Error loading ") + 
                    shared_lib->string() + 
                    "(error code = " + 
                    std::to_string(error.value()) + 
                    "). Did you properly setup the environment using \"grunk virtualrunenv\"?\n"
                );
            }

            if (lib.has("create_grunk_plugin")) {
                insert_plugin(std::move(lib));
            }
        } catch (std::bad_alloc e)
        {
            throw std::runtime_error(
                std::string("Cannot load grunk plugin ") + 
                shared_lib->string() 
                + "\n" + e.what()
            );
        }
    } else {
        throw std::runtime_error(
            std::string("Cannot find plugin \"") + name + "\" in the search path.\n"
        );
    }
}

void PluginRegistry::unload(std::string const& name)
{
    //TODO: erase all functions and types starting with "name::" from 
    //function and type registry
    loaded_plugins.erase(name);
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
