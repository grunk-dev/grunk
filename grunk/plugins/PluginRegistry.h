#pragma once

//TODO: Put this in CMAKE?
#define BOOST_DLL_USE_STD_FS 1

#include <boost/dll/shared_library.hpp>
#include <unordered_map>
#include <filesystem>
#include "IPlugin.h"

namespace grunk {

/**
 * @brief The PluginRegistry manages all registered plugins
 * 
 */
class PluginRegistry {

    // a bit hacky: We mustn't close a loaded dll, because our local static type registry contains
    // pointers to TypeDescriptors defined in the plugin.
    using PluginMap = std::unordered_map<std::string, boost::dll::shared_library>;

public:

    /**
     * @brief Construct a new PluginRegistry object given the path to a 
     * directory containing the plugins
     * 
     * @param plugins_dir path to a directory containing the plugins
     */
    PluginRegistry(const std::filesystem::path& plugins_dir);

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
     * @brief Destroy the Plugin Registry object
     */
    ~PluginRegistry();

private:

    /**
     * @brief loads all plugins in the directory provided to the construcotr
     */
    void load_all();

    /**
     * @brief inserts a plugin into the plugin map
     * 
     */
    void insert_plugin(BOOST_RV_REF(boost::dll::shared_library) lib);

    std::filesystem::path plugins_directory;
    PluginMap loaded_plugins;
};

} //namespace grunk