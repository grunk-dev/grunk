#pragma once

#define BOOST_DLL_USE_STD_FS 1

#include <boost/dll/shared_library.hpp>
#include <unordered_map>
#include <deque>
#include <filesystem>

#include "IPlugin.hpp"

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
     * @brief loads all plugins in the directory provided to the construcotr
     */
    void load_all();

    /**
     * @brief unloads all currently loaded plugins. Note that
     * it does not clear the registered types and functions.
     */
    void unload_all();

    /**
     * @brief 
     * 
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

    Path path;
    PluginMap loaded_plugins;
};

PluginRegistry& get_plugin_registry();

} //namespace grunk