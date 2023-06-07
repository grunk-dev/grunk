/**
 * @file IPlugin.hpp
 *
 * This file contains the implementation of the plugin interface 
 *
 * @defgroup plugin
 * 
 */

#pragma once

//TODO: Put this in CMAKE?
#define BOOST_DLL_USE_STD_FS 1

#include <reflect/reflect.hpp>
#include <boost/dll/alias.hpp>


namespace grunk {


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

    /**
     * @brief factory function for derived classes
     * 
     * @tparam Plugin The plugin to be created. Must be default-constructible and derived from IPlugin
     * @return std::unique_ptr<IPlugin> the created plugin
     */
    template <typename Plugin>
    inline static std::unique_ptr<IPlugin> create() {
        static_assert(std::is_default_constructible_v<Plugin>, "Can only create default-constructible plugins.");
        static_assert(std::is_base_of_v<IPlugin, Plugin>, "Can only create plugins derived from IPlugin");
        return std::make_unique<Plugin>();
    };

    /**
     * @brief register_function is a function alias for Reflect::RegisterFunction<F> from
     * the reflect library
     *
     * @tparam F the type of fucntion to be reflected/registered
     *
     */
    template <typename F>
    void register_function(F&& f, std::string const& function_name, std::string const& doc = "") const
    {
        std::string prefix = name().empty()? "" : name() + "::";
        return reflect::register_function(std::forward<F>(f), prefix + function_name, doc);
    }

    /**
     * @brief register_type is a function alias for Reflect::Reflect<T> from
     * the reflect library
     *
     * @tparam T the type to be reflected/registered
     *
     * @ingroup plugin
     */
     template <typename T, template <typename...> typename... Ptrs>
     decltype(auto) register_type(std::string const& type_name) const
     {
        std::string prefix = name().empty()? "" : name() + "::";
        return reflect::register_type<T, Ptrs...>(prefix + type_name);
     }
};

} //namespace grunk

/**
 * @brief The macro GRUNK_REGISTER_PLUGIN must be used by plugin authors to 
 * register the derived class from IPlugin with the plugin registry.
 *
 * @ingroup plugin
 * 
 */
#if (defined(_MSC_VER) &&_MSC_VER < 1920)
namespace {

// MSVC-2017 Workaround from https://stackoverflow.com/questions/51967446/reinterpret-cast-cannot-convert-from-overloaded-function-to-intptr-t-with
// this really does nothing:
template<class R, class...Args>
R(*to_fptr( R(*f)(Args...) ))(Args...) {
    return f;
}

} // anonymous namespace
#define GRUNK_REGISTER_PLUGIN(name) BOOST_DLL_ALIAS(to_fptr(grunk::IPlugin::create<name>), create_grunk_plugin)
#else
#define GRUNK_REGISTER_PLUGIN(name) BOOST_DLL_ALIAS(grunk::IPlugin::create<name>, create_grunk_plugin)
#endif
