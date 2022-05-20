#pragma once

//TODO: Put this in CMAKE?
#define BOOST_DLL_USE_STD_FS 1

#include <boost/dll/shared_library.hpp>
#include <map>
#include <filesystem>
#include "api.h"

namespace grunk {

class PluginRegistry {

    // a bit hacky: We mustn't close a loaded dll, because our local static type registry contains
    // pointers to TypeDescriptors defined in the plugin.
    using PluginMap = std::map<std::string, boost::dll::shared_library>;

public:

    PluginRegistry(const std::filesystem::path& plugins_dir);

    void print_plugins() const;

    std::size_t count() const;

    ~PluginRegistry();

private:

    void load_all();

    void insert_plugin(BOOST_RV_REF(boost::dll::shared_library) lib);

    std::filesystem::path plugins_directory;
    PluginMap loaded_plugins;
};

} //namespace grunk