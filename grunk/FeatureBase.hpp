#pragma once

#include <parametric/core.hpp>

namespace grunk {

/**
 * @brief A base class used by Feature<T> and the template specialization
 * Feature<DynamicObject> aka DynamicFeature.
 *
 * This class implements the common interface for all template realizations.
 * 
 * @tparam T The wrapped type of the feature
 *
 * @ingroup advanced
 */
template <typename T>
class FeatureBase {
public:

    template <typename F, typename... Args>
    friend class Action;

    /**
     * @brief Construct a new FeatureBase object from an instance of type T
     * 
     * @param id The id of the Feature
     * @param t The instance to be wrapped inside this feature
     */
    FeatureBase(std::string const& id, T&& t)
     : m_param(parametric::new_param(std::forward<T>(t)), id)
    {}

    /**
     * @brief Construct a new FeatureBase object from a parametric::param<T>
     * 
     * @param p a parametric::param<T>
     */
    FeatureBase(parametric::param<T>&& p)
     : m_param(p)
    {}

    /**
     * @brief returns true if the Feature's cache contains a valid value
     * and false, if it doesn't.
     *
     * A feature is invalid, if its ancestors are invalid, that is if the
     * computations upwards in the feature tree have not been performed, or
     * if an ancestor has been invalidated by a parameter change.
     * 
     * @return true if the cache contains a value
     * @return false if the cache does not contain a value
     */
    bool is_valid() const
    {
        return m_param.is_valid();
    }

    /**
     * @brief returns a const reference to the contained value.
     *
     * This triggers the computation of (part of) the feature tree, if 
     * the feature has been invalid before.
     * 
     * @return T const& reference to the wrapped object
     */
    T const& value() const
    {
        return m_param.value();
    }

    /**
     * @brief returns the id of the feature
     * 
     * @return std::string 
     */
    std::string id() const {
        return m_param.id();
    }

    /**
     * @brief set_id sets the id
     * @param s The id of the feature
     */
    void set_id(std::string const& s) {
        m_param.set_id(s);
    }

    /**
     * @brief returns a non-const reference to the feature. 
     * 
     * This triggers invalidation of all descendents in the feature tree.
     *
     * When changing a non-independent feature maually using access_value, 
     * any changes in the ancestors get precedence: A following change of 
     * an ancestor of this feature will trigger the invalidation of this 
     * feature and the manual change will be overwritten.
     * 
     * @return T& reference to the wrapped object.
     */
    T& access_value()
    {
        return m_param.change_value();
    }

    /**
     * @brief Set the value of the feature. 
     * 
     * @param other 
     */
    void set_value(T const& other)
    {
        m_param.set_value(other);
    }

    /**
     * @brief returns a 
     * 
     * @return parametric::param<T> const& 
     */
    parametric::param<T> const& param() const {
        return m_param;
    }

protected:

    /**
     * @brief m_param FeatureBase is a wrapper around a parametric::param
     */
    parametric::param<T> m_param;
};

} // namespace grunk