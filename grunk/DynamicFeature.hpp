/**
 * @file DynamicFeature.hpp
 *
 * This file contains the template specialization of Feature for reflect::DynamicObjects
 */

/**
 * @defgroup dynamic Functions and Classes for dynamic mode
 */

/**
 * @defgroup dynamic_advanced Advanced Functions and Classes for dynamic mode
 */


#pragma once

#include <grunk/Feature.hpp>

namespace grunk {

/**
 * @brief template specialization of Feature for reflect::DynamicObjects
 *
 * In addition to the dependency management provided by the parametric library,
 * this class provides an interface to retrieve data members and invoke member
 * functions on the wrapped object and registering this action in the feature 
 * tree.
 * 
 * @ingroup dynamic  
 */
template <>
class Feature<reflect::DynamicObject> : public FeatureBase<reflect::DynamicObject>
{
    
public:

    /**
     * @brief Construct a new DynamicFeature given the string representation
     * of a reflected type and constructor arguments.
     *
     * This constructs a DynamicObject given constructor arguments and wraps 
     * it in a feature instance. See also make_rto.
     * 
     * @tparam Args The constructor arguments
     * @param id The id of the Feature
     * @param typeName The string representation of the reflected type
     * @param args The constructor arguments
     */
    template <typename... Args>
    Feature(std::string const& id, std::string const& typeName, Args const&... args);

    /**
     * @brief Construct a new DynamicFeature given a parametric::param<T>.
     *
     * @param p The parametric::param<T> to be wrapped in a Feature
     * @param t An optional type descriptor, so that we can know the type of the held feature
     *          without having to evaluate the parametric tree.
     */
    Feature(parametric::param<reflect::DynamicObject>&& p, reflect::TypeDescriptor const* t = nullptr);
    //note: The type descriptor is optional to be consistent with the compile time action.

    /**
     * @brief Construct a new DynamicFeature given an reflect::DynamicObject
     * 
     * @param id The id of the Feature
     * @param o The input reflect::DynamicObject
     */
    explicit Feature(std::string const& id, reflect::DynamicObject&& o);

    /**
     * @brief Converting constructor from a Feature<T>, where T is not
     * a reflect::DynamicObject
     *
     * <b>Caution:</b> The converted DynamicFeature will hold a reference
     * to the value held by the input Feature. This means that the input Feature
     * must outlive the converted DynamicFeature. If this is not the case, it is
     * better to explicitly construct a new DynamicFeature instead of using
     * this converting constructor.
     * 
     * @tparam T The type wrapped by the incoming Feature<T>
     * @param f The input feature to be converted to a DynamicFeature
     */
    template <typename T,
              typename = std::enable_if_t<!std::is_same_v<reflect::DynamicObject, T>>
    >
    Feature(Feature<T> const& f);

    /**
     * @brief converts a RuntimFeature to a Feature<T>
     * 
     * @tparam T The type of the object to be wrapped
     * @return Feature<T> The converted Feature<T>
     */
    template <typename T, typename = std::enable_if_t<!std::is_same_v<T, reflect::DynamicObject>>>
    operator Feature<T>() const;

    /**
     * @brief get_type_descriptor returns a pointer to the type descriptor class of the held type.
     * This class holds information about the name of the type, base classes, convesion operators,
     * data members and member functions
     * @return type descriptor of the type of the held value.
     */
    reflect::TypeDescriptor const* get_type_descriptor() const {
        return type_descriptor;
    }

private:

    reflect::TypeDescriptor const* type_descriptor {nullptr};

};

/**
 * @brief C++ 17 deduction guides for DynamicFeature
 * 
 * @tparam Args The constructor arguments
 */
template <typename... Args>
Feature(std::string const&, Args&&...) -> Feature<reflect::DynamicObject>;

/**
 * @brief typedef for DynamicFeature
 * @ingroup dynamic
 */
using DynamicFeature = Feature<reflect::DynamicObject>;

}

#include <grunk/DynamicFeature.inl>
