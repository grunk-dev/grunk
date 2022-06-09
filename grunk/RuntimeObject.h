#pragma once

#include "reflect/Reflect.hpp"
#include <type_traits>

namespace grunk {

    class RuntimeObject {
    public:

        RuntimeObject() = default;

        template <typename... CtorArgs>
        explicit RuntimeObject(Reflect::TypeDescriptor const& descriptor, CtorArgs&&... args)
         : type_info(&descriptor)
         , object(type_info->GetConstructor<CtorArgs...>()->NewInstance(std::forward<CtorArgs>(args)...))
        {}


        template <typename T, 
                  typename = typename std::enable_if<!std::is_same<std::decay_t<T>, Reflect::TypeDescriptor>::value>::type,
                  typename = typename std::enable_if<!std::is_reference_v<T>>::type,
                  typename = typename std::enable_if<!std::is_pointer_v<T>>::type
        > 
        RuntimeObject(T const& t)
         : type_info(Reflect::Resolve<T>())
         , object(t)
        {}

        // some convenience funcs to set and get dataMembers as RuntimeObjects

        // it would be really cool to be able to set by reference
        RuntimeObject Get(std::string const& memberName) const;

        void Set(std::string const& memberName, RuntimeObject const&);

        template <typename... Args>
        RuntimeObject Invoke(std::string const& name, Args&&... args){
            auto* fun = type_info->GetMemberFunction(name);
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
        return RuntimeObject(*Reflect::Resolve(typeName), std::forward<Args>(args)...);
    }

} //namespace grunk