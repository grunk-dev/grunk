/**
 * @file RuntimeObject.h
 */

#pragma once

#include "reflect/Reflect.hpp"

#include <cassert>
#include <type_traits>
#include <typeinfo>
#include <stdexcept>

namespace grunk {


    /**
     * @brief This class does ...
     *
     * A more detailed description of this class can be found here.
     */
    class RuntimeObject {
    public:

        RuntimeObject() = delete;

        template <typename... CtorArgs>
        explicit RuntimeObject(Reflect::TypeDescriptor const& descriptor, CtorArgs&&... args)
         : type_info(&descriptor)
         , object(type_info->GetConstructor<CtorArgs...>()->NewInstance(std::forward<CtorArgs>(args)...))
        {
        }


        template <typename T,
                  typename = typename std::enable_if<!std::is_same<std::decay_t<T>, Reflect::TypeDescriptor>::value>::type,
                  typename = typename std::enable_if<!std::is_reference_v<T>>::type,
                  typename = typename std::enable_if<!std::is_pointer_v<T>>::type
        >
        explicit RuntimeObject(T const& t)
         : type_info(Reflect::Resolve<T>())
         , object(t)
        {
            assert(type_info != nullptr);
            if (type_info == nullptr || type_info->GetName() == ""){
                throw std::domain_error(std::string("Constructor for RuntimeObject called with unregistered type ") + typeid(t).name());
            }
        }

        // some convenience funcs to set and get dataMembers as RuntimeObjects

        // it would be really cool to be able to set by reference
        RuntimeObject Get(std::string const& memberName) const;

        template <typename T>
        void Set(std::string const& memberName, T const& t)
        {
            //this is just a convenience wrapper to hide the explicit conversion
            Set(memberName, (RuntimeObject)t);
        };

        void Set(std::string const& memberName, RuntimeObject const& obj);

        template <typename... Args>
        RuntimeObject Invoke(std::string const& name, Args&&... args)
        {
            
            auto* fun = type_info->GetMemberFunction(name);
            
            if (!fun) {
                throw std::invalid_argument("RuntimeObject::Invoke: No member function \"" + name + "\" found for Type \"" + type_info->GetName() + "\"");
            }

            if constexpr ( (std::is_same_v<std::decay_t<Args>, RuntimeObject> && ...) ) { // cleaner would be a per-arg conversion
                return RuntimeObject(fun->GetReturnType(),
                                     fun->Invoke(object, args.object...));
            } else {
                return RuntimeObject(fun->GetReturnType(),
                                     fun->Invoke(object, RuntimeObject(args).object...));
            }
        }

        /// some convenience funcs for casting
        template <typename T>
        T cast() {
            if constexpr (std::is_reference_v<T>) {
                using Type = std::remove_reference_t<T>;
                return *std::any_cast<Type>(&object);
            }
            else if constexpr (std::is_pointer_v<T>) {
                using Type = std::remove_pointer_t<T>;
                return std::any_cast<Type>(&object);
            }
            else {
                return std::any_cast<T>(object);
            }
        }

        template <typename T>
        T cast() const {
            if constexpr (std::is_reference_v<T>) {
                using Type = std::remove_reference_t<T>;
                return *std::any_cast<Type const>(&object);
            }
            else if constexpr (std::is_pointer_v<T>) {
                using Type = std::remove_pointer_t<T>;
                return std::any_cast<Type const>(&object);
            }
            else {
                return std::any_cast<T>(object);
            }
        }

        template <typename T>
        T GetAs(std::string const& memberName) const {
            return Get(memberName).cast<T>();
        }

        Reflect::TypeDescriptor const* GetTypeInfo() const;

    private:
        Reflect::TypeDescriptor const* type_info  {nullptr};
        std::any object;

        explicit RuntimeObject(Reflect::TypeDescriptor const* ti, std::any const& obj)
         : type_info{ti}
         , object{obj}
         {}
    };

    template <typename... Args>
    inline RuntimeObject make_rto(std::string const& typeName, Args&&... args)
    {
        auto* descr = Reflect::Resolve(typeName);

        if (!descr) {
            throw std::invalid_argument("make_rto: No type with name\"" + typeName + "\" found in static TypeRegistry.");
        }

        if (!descr->GetConstructor<Args...>()) {

            std::string error = "make_rto: No known constructor of type \""
                                + descr->GetName()
                                + "\" accepts the given argument(s)";
            if constexpr (sizeof...(Args) == 0) {
                error = "make_rto: No constructor of type \""
                        + descr->GetName()
                        + "\" accepts zero argument(s)";
                throw std::invalid_argument(error + ".");
            } else if constexpr (sizeof...(Args) == 1) {
                throw std::invalid_argument(error + " " + typeid(args).name()...);
            } else {
                throw std::invalid_argument(error
                                            + (... + (", " + std::string(typeid(args).name())))
                                            + "."
                );
            }
        }

        return RuntimeObject(*descr, std::forward<Args>(args)...);
    }

} //namespace grunk
