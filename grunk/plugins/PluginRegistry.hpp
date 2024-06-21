#pragma once

#define BOOST_DLL_USE_STD_FS 1

#include "IPlugin.hpp"

#include <boost/dll/shared_library.hpp>
#include <unordered_map>
#include <deque>
#include <filesystem>

namespace grunk {

/**
 * @brief The PluginRegistry manages all registered plugins
 * 
 */
class PluginRegistry {

    friend PluginRegistry& get_plugin_registry();

    /**
     * @brief Construct a new PluginRegistry object given the path to a 
     * directory containing the plugins
     * 
     * @param plugins_dir path to a directory containing the plugins
     */
    PluginRegistry();

public:

    PluginRegistry(PluginRegistry const&) = delete;
    PluginRegistry& operator=(PluginRegistry const&) = delete;
    PluginRegistry(PluginRegistry&&) = delete;
    PluginRegistry& operator=(PluginRegistry&&) = delete;

    /**
     * @brief Destroy the Plugin Registry object
     */
    ~PluginRegistry();

    /**
     * @brief The Entry struct is a composition class of the loaded library and the plugin initialized from it.
     */
    struct Entry {
        boost::dll::shared_library library;
        std::unique_ptr<IPlugin> plugin;
    };

    // a bit hacky: We mustn't close a loaded dll, because our local static type registry contains
    // pointers to TypeDescriptors defined in the plugin.
    using PluginMap = std::unordered_map<std::string, Entry>;
    using Path = std::deque<std::filesystem::path>;

    /**
     * @brief prepends the current search path for plugins
     *
     * @param path the directory that shall be added to the
     * search path
     */
    void prepend_path(std::string const& path);

    /**
     * @brief appends the current search path for plugins
     *
     * @param path the directory that shall be added to the
     * search path
     */
    void append_path(std::string const& path);

    /**
     * @brief returns the currently active environment, if any
     * 
     * @return std::optional<std::string> 
     */
    std::optional<std::string> active_environment() const;

    /**
     * @brief prepends the environment path to the current search
     * paths for a given grunk environment
     * 
     * @param env_name 
     */
    void activate_environment(std::string const& env_name);

    void deactivate_environment();

    /**
     * @brief populates the search path for plugins from the environmentall 
     * variables PATH, LD_LIBRARY_PATH and DYLD_LIBRARY_PATH
     */
    void populate_path_from_env();

    /**
     * @brief prints the loaded plugins to console
     */
    void print_plugins() const;

    /**
     * @brief returns the number of loaded plugins
     * 
     * @return std::size_t the number of loaded plugins
     */
    std::size_t count() const;

    /**
     * @brief load a plugin of a given name.
     * 
     * @param name name of the plugin
     */
    void load(std::string const& name);

    /**
     * @brief unloads a plugin of a given name
     * 
     * @param name 
     */
    void unload(std::string const& name);

    /**
     * @brief unloads all currently loaded plugins. Note that
     * it does not clear the registered types and functions.
     */
    void unload_all();

    /**
      * @brief returns the plugins
      * @return The loaded plugins
      */
     PluginMap const& plugins() const
     {
        return loaded_plugins;
     }

private:

    /**
     * @brief inserts a plugin into the plugin map
     * 
     */
    void insert_plugin(BOOST_RV_REF(boost::dll::shared_library) lib);

    // find a .so or .dll file in the path that contains the passed argument as substring
    std::optional<std::filesystem::path> find_shared_lib(std::string_view name) const;

    // gets the path containing the runtime dependencies of a given grunk environment
    static std::filesystem::path get_environment_path(std::string const& env_name);

    std::optional<std::string> m_active_environment;
    Path path; //search path for plugins
    PluginMap loaded_plugins;
};

PluginRegistry& get_plugin_registry();

} //namespace grunk
