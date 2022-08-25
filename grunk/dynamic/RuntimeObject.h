/**
 * @file RuntimeObject.h
 *
 * This file contains the declaration of RuntimeObject
 */

/**
 * @defgroup dynamic Functions and Classes for dynamic mode
 */

/**
 * @defgroup dynamic_advanced Advanced Functions and Classes for dynamic mode
 */

#pragma once

#include "reflect/Reflect.hpp"

#include <cassert>
#include <type_traits>
#include <typeinfo>
#include <stdexcept>

namespace grunk {

    /**
     * @brief This class represents an instance of a dynamic type.
     *
     * It lies at the center of grunk's dynamic type system. A RuntimeObject
     * wraps an std::any together with a const pointer to a type description,
     * which provides some additional functionality such as retrieving data
     * members or invoking member functions.
     *
     * This class provides an additional interface around the functionality
     * provided by the reflect library.
     *
     * @ingroup dynamic_advanced
     */
    class RuntimeObject {
    public:
        /**
         * @brief A RuntimeObject is not default constructable
         * 
         */
        RuntimeObject() = delete;

        template <typename... Args>
        friend RuntimeObject make_rto(std::string const& typeName, Args&&... args);

    private:

        /**
         * @brief Construct a new RuntimeObject object, given a Reflect::TypeDescriptor
         * and constructor arguments.
         * 
         * @tparam CtorArgs The types used by the constructor of the represented type
         * @param descriptor The type description of the represented type
         * @param args The constructor arguments
         */
        template <typename... CtorArgs>
        explicit RuntimeObject(Reflect::TypeDescriptor const& descriptor, CtorArgs&&... args)
         : type_info(&descriptor)
         , object(type_info->GetConstructor<CtorArgs...>()->NewInstance(std::forward<CtorArgs>(args)...))
        {
        }

    public:

        /**
         * @brief Construct a new RuntimeObject object, given an instance of 
         * a (known) type T.
         * 
         * @tparam T The (known) type of the object to be wrapped
         * @param t The instance to be wrapped
         */
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

        /**
         * @brief Retrieves a data member given the name of the data member
         *
         * This only works, if the type has been reflected together with the 
         * dta member
         * 
         * @param memberName string representation of the data member
         * @return RuntimeObject the data member, wrapped in a RuntimeObject
         */
        RuntimeObject get(std::string const& memberName) const;

        /**
         * @brief sets a data member given its name and a value
         * 
         * @tparam T (known) type of the data member
         * @param memberName string representation of the data member
         * @param t new value of the data member
         */
        template <typename T>
        void set(std::string const& memberName, T const& t)
        {
            //this is just a convenience wrapper to hide the explicit conversion
            set(memberName, (RuntimeObject)t);
        };

        /**
         * @brief sets a data member given a RuntimeObject
         * 
         * @param memberName string representation of the data member
         * @param obj new value of the data member
         */
        void set(std::string const& memberName, RuntimeObject const& obj);

        /**
         * @brief invokes a member function (const version)
         * 
         * @tparam Args types of the arguments expected by the member function
         * @param name string representation of the member function
         * @param args arguments for the member function (can be of known type or RuntimeObjects)
         * @return RuntimeObject The return value of the member function
         */
        template <typename... Args>
        RuntimeObject invoke(std::string const& name, Args&&... args) const
        {
            auto* fun = type_info->GetMemberFunction(name);
            
            if (!fun) {
                throw std::invalid_argument("RuntimeObject::Invoke: No member function \"" + name + "\" found for Type \"" + type_info->GetName() + "\"");
            }

            return RuntimeObject(fun->GetReturnType(),
                                 fun->Invoke(object, to_rto(args).object...));
        }

        /**
         * @brief invokes a member function
         * 
         * @tparam Args types of the arguments expected by the member function
         * @param name string representation of the member function
         * @param args arguments for the member function (can be of known type or RuntimeObjects)
         * @return RuntimeObject The return value of the member function
         */
        template <typename... Args>
        RuntimeObject invoke(std::string const& name, Args&&... args)
        {
            auto* fun = type_info->GetMemberFunction(name);
            
            if (!fun) {
                throw std::invalid_argument("RuntimeObject::Invoke: No member function \"" + name + "\" found for Type \"" + type_info->GetName() + "\"");
            }

            return RuntimeObject(fun->GetReturnType(),
                                 fun->Invoke(object, to_rto(args).object...));
        }

        /**
         * @brief casts a RuntimeObject to a specific type
         *
         * This can cast to a reference, pointer or a copy, depending 
         * on the template parameter T
         *
         * throws std::bad_any_cast if the casting fails.
         * 
         * @tparam T The type to be cast to
         * @return T The casted value
         */
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

        /**
         * @brief casts a RuntimeObject to a specific type (const version)
         *
         * This can cast to a reference, pointer or a copy, depending 
         * on the template parameter T
         *
         * throws std::bad_any_cast if the casting fails.
         * 
         * @tparam T The type to be cast to
         * @return T The casted value
         */
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

        /**
         * @brief a convenience function to get a data member and immediatly
         * cast the returned value to a concrete type
         * 
         * @tparam T The type the return value shall be cast to
         * @param memberName The string representation of the data member
         * @return T The data member
         */
        template <typename T>
        T get_as(std::string const& memberName) const {
            return get(memberName).cast<T>();
        }

        /**
         * @brief returns the type description as provided by the reflect library
         * 
         * @return Reflect::TypeDescriptor const* A constant pointer to the type description
         */
        Reflect::TypeDescriptor const* get_type_info() const;

    private:
        Reflect::TypeDescriptor const* type_info  {nullptr};
        std::any object;

        /**
         * @brief Construct a new RuntimeObject object given a type descriptor 
         * and an std::any, as provided by e.g. a reflect::Constructor
         * 
         * @param ti the type descriptor
         * @param obj the instance as an std::any
         */
        explicit RuntimeObject(Reflect::TypeDescriptor const* ti, std::any const& obj)
         : type_info{ti}
         , object{obj}
         {}

        /**
         * @brief A convenience function to turn objects into RuntimeObjects,
         * if and only if they aren't RuntimeObjects already. This is used
         * when passing in the arguments to member functions in invoke
         * 
         * @tparam T The type of the input argument
         * @param t The input argument
         * @return RuntimeObject The input argument as a RuntimeObject
         */
        template<typename T>
        static RuntimeObject to_rto(T const& t){
            if constexpr ( std::is_same_v<std::decay_t<T>, RuntimeObject> )
            {
                return t;
            }
            return RuntimeObject(t);
        };

    };

    /**
     * @brief A factory function to create RuntimeObject instances, given
     * a string representation of a type as provided to the reflect library 
     * as well as constructor arguments
     *
     * throws std::invalid_argument if the string representation is not found
     * to be a reflected type, or if no known constructor given the argument types
     * exists or the number of arguments does not match.
     * 
     * @tparam Args The types of the arguments passed to a constructor of the 
     *              represented type
     * @param typeName The string representation of the type to be created
     * @param args The constructor arguments of the type
     * @return RuntimeObject the created RuntimeObject
     *
     * @ingroup dynamic_advanced
     */
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
