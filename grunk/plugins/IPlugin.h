/**
 * @file IPlugin.h
 *
 * This file contains the implementation of the plugin interface 
 *
 * @defgroup plugin
 * 
 */

#pragma once

//TODO: Put this in CMAKE?
#define BOOST_DLL_USE_STD_FS 1

#include <reflect/Reflect.hpp>
#include <boost/dll/alias.hpp> 

/**
 * @brief The macro GRUNK_REGISTER_PLUGIN must be used by plugin authors to 
 * register the derived class from IPlugin with the plugin registry.
 *
 * @ingroup plugin
 * 
 */
#define GRUNK_REGISTER_PLUGIN(name) \
    static_assert(std::is_default_constructible_v<name>); \
    namespace detail { \
    std::unique_ptr<name> create() { \
        return std::make_unique<name>(); \
    }; \
    } \
    BOOST_DLL_ALIAS(detail::create, create_grunk_plugin)

namespace grunk {

/**
 * @brief register_type is a function alias for Reflect::Reflect<T> from
 * the reflect library
 * 
 * @tparam T the type to be reflected/registered
 *
 * @ingroup plugin
 */
 template <typename T>
 decltype(auto) register_type(std::string const& name)
 {
    return Reflect::Reflect<T>(name);
 }

/**
 * @brief register_function is a function alias for Reflect::RegisterFunction<F> from
 * the reflect library
 * 
 * @tparam F the type of fucntion to be reflected/registered
 *
 * @ingroup plugin
 */
template <typename F>
void register_function(F&& f, std::string const& name, std::string const& doc = "")
{
    return Reflect::RegisterFunction(std::forward<F>(f), name, doc);
}


/**
 * @brief The Plugin interface. All plugin authors must derive their plugin 
 * from this class and then register their plugin via the GRUNK_REGISTER_PLUGIN
 * macro.
 * 
 * @ingroup plugin
 */
struct IPlugin
{
    /**
     * @brief This function must return the name of the plugin
     * 
     * @return std::string The name of the plugin
     */
    virtual std::string name() const = 0;

    /**
     * @brief This function must return the version of the 
     * plugin in accordance to SemVer 2.0
     * 
     */
     virtual std::string version() const = 0;

    /**
     * @brief plugin authors should call register_type and register_function 
     * in the body of this function. It is called as soon as the plugin is
     * loaded.
     * 
     */
    virtual void init() const {};

    /**
     * @brief Destroy the IPlugin object
     */
    virtual ~IPlugin(){}
};

} //namespace grunk