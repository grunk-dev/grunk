#include <algorithm>
#include <grunk/plugins/PluginRegistry.hpp>
#include <grunk/common/init.hpp>
#include <grunk/common/common_functions.hpp>
#include <boost/dll/import.hpp>
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace grunk {

PluginRegistry::PluginRegistry()
{
    // make sure built-in types are registered
    grunk::init();
}

void PluginRegistry::prepend_path(std::string const& dir)
{
    std::filesystem::path p(dir);
    if (std::filesystem::is_directory(p)) {
        path.push_front(p);
    }
}

std::filesystem::path PluginRegistry::get_environments_root() 
{
    std::filesystem::path p(get_home_dir());
    p /= ".grunk";
    p /= "envs";
    return p;
}

std::filesystem::path PluginRegistry::get_environment_path(std::string const& env_name)
{
    auto p = get_environments_root();
    p /= env_name;
#if defined(_WIN32) || defined(_WIN64)
    p /= "bin";
#else 
    p /= "lib";
#endif
    return p;
}

std::optional<std::string> PluginRegistry::active_env() const
{
    return m_active_environment;
}

void PluginRegistry::activate_env(std::string const& env_name)
{
    if (active_env()) {
        deactivate_env();
    }
    auto p = get_environment_path(env_name);
    if (std::filesystem::is_directory(p)) {
        if (std::find(path.begin(), path.end(), p) == path.end()) {
            path.push_front(p);
        }
    }
    m_active_environment = env_name;
}

void PluginRegistry::deactivate_env() {
    if (!active_env()) {
        return;
    } else {
        auto p = get_environment_path(*active_env());
        auto it = std::remove(path.begin(), path.end(), p);
        path.erase(it);
        m_active_environment = std::nullopt;
    }
}

void PluginRegistry::load_env(std::string const& env_name)
{
    activate_env(env_name);
    for (auto const& ref : env_plugins(env_name)) {
        load(grunk::split(ref, "/")[0]);
    }
}

void PluginRegistry::unload_env(std::string const& env_name)
{
    for (auto const& ref : env_plugins(env_name)) {
        load(grunk::split(ref, "/")[0]);
    }
    if (active_env() && *active_env() == env_name) {
        deactivate_env();
    }
}

std::vector<std::string> PluginRegistry::envs()
{
    std::vector<std::string> subdirs;

    for (auto const& entry : std::filesystem::directory_iterator(get_environments_root())) {
        if (entry.is_directory()) {
            subdirs.push_back(entry.path().filename().string());
        }
    }
    return subdirs;
}

std::vector<std::string> PluginRegistry::env_plugins(std::string const& name)
{
    std::vector<std::string> plugins;
    auto filename = get_environments_root();
    filename /= name;
    filename /= "conanfile.txt";
    std::ifstream file(filename);

    if(!file.is_open()) {
        throw std::runtime_error(std::string("Could not open the file ") + filename.string());
    }

    std::string line;
    bool in_requires_section = false;

    while (std::getline(file, line)) {
        if (!in_requires_section && line.rfind("[requires]", 0) == 0) {
            in_requires_section = true;
            continue;
        }

        if (in_requires_section) {
            if (line.find('[') == 0) {
                break;
            }

            if (!line.empty()) {
                plugins.push_back(line);
            }
        }
    }
    file.close();
    return plugins;
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
#if defined (__WIN32__) || defined (__WIN64__) || defined(__CYGWIN__) || defined (_WIN32) || defined(_WIN64)
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
            boost::dll::shared_library lib(
                *shared_lib, 
                error, 
                boost::dll::load_mode::load_with_altered_search_path
            );
            if (error) {
                throw std::runtime_error(
                    std::string("Error loading ") + 
                    shared_lib->string() + 
                    "(error code = " + 
                    std::to_string(error.value()) + 
                    "). Did you properly setup a grunk environment using the command line?\n"
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
