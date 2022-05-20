#pragma once

//TODO: Put this in CMAKE?
#define BOOST_DLL_USE_STD_FS 1

#include <boost/dll/alias.hpp> 

#include "reflect/Reflect.hpp"

// This requires #include<memory>
#define GRUNK_REGISTER_PLUGIN(name) \
    static_assert(std::is_default_constructible_v<name>); \
    namespace detail { \
    std::unique_ptr<name> create() { \
        return std::make_unique<name>(); \
    }; \
    } \
    BOOST_DLL_ALIAS(detail::create, create_grunk_plugin)

namespace grunk {

struct IPlugin
{
    virtual std::string name() const = 0;
    virtual void register_types() const {};
    virtual ~IPlugin(){}
};

} //namespace grunk