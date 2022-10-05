#pragma once

#include <initializer_list>
#include <unordered_map>

#include "RuntimeAlgorithm.h"

namespace grunk {

/**
 * @brief The FunctionRegistry can be used to dynamically register functions
 * for use in a RuntimeAlgorithm.
 *
 * @ingroup dynamic_advanced
 */
class FunctionRegistry
{
public:

    /**
     * @brief An entry in the RuntimeRegistry. It consists of a factory function
     * that returns a RuntimeAlgorithmPtr given an std::initializar_list of 
     * RuntimeFeatures and an optional documentation for the registered function
     * 
     */
    struct Entry {

        /**
         * @brief Construct a new Entry object given a factory function and a string
         * documenting the function
         * 
         * @tparam F The type of the function to be registered
         * @param algo_factory The factory function
         * @param doc The string with the documentation of the function to be registered
         */
        template <typename F>
        Entry(F const& algo_factory, std::string doc = "")
         : factory(algo_factory)
         , documentation(doc)
        {}

        std::function<parametric::compute_node_ptr<RuntimeAlgorithm>(std::initializer_list<Feature<Reflect::DynamicObject>> const&)> factory;
        std::string documentation;
    };

    /**
     * @brief FunctionRegistry cannot be default constructed
     */
    FunctionRegistry() = default;

    /**
     * @brief register a new function given a string as key
     * 
     */
    void insert(std::string const&, Entry const&);

    /**
     * @brief retrieve a function from the registry given a string as key
     * 
     * @return Entry const& an entry in the function registry
     */
    Entry const& operator[](std::string const&) const;

    /**
     * @brief clears all entries from the function registry
     */
    void clear();

private:

    std::unordered_map<std::string, Entry> algorithmMap;

};

/**
 * @brief lazy creation of static algorithm registry. 
 *
 * This creates a "Meyer's singleton".
 * 
 * @return FunctionRegistry& a reference to the static function registry
 *
 * @ingroup dynamic_advanced
 */
FunctionRegistry& get_function_registry();

/**
 * @brief function to register functions in the static function registry.
 *
 * This function is type of grunk's dynamic type system. You can register functions
 * unknown at compile-time via plugins and generate RuntimeAlgorithms from it.
 * 
 * @tparam F The type of the function to be registered.
 * @param f The function to be registered
 * @param name A string identifier for the function
 * @param doc An optional documentation string for the function
 */
template <typename F>
void register_function(F&& f,
                      std::string const& name,
                      std::string doc = "")
{
    RuntimeFunction<F> func(f);
    auto& registry = get_function_registry();

    registry.insert(
        name,
        {
            [=](auto& args) {
                return eval(func, args);
            },
            doc
        }
    );
}

/**
 * @brief Given a string identifier of a function, that has previously been registered
 * in the static function registry as well as input features of the feature tree, 
 * this function represents the evaluation of the registered function when it gets 
 * passed the input features.
 *
 * This function accepts features as arguments for the functions, as well
 * as instances that are not wrapped in features. Internally, the latter will
 * be wrapped in an unnamed/anonymous feature
 * 
 * @tparam Args The types of the arguments expected by the registered function
 * @param name The string identifier of the registered function
 * @param args The input Features
 * @return RuntimeAlgorithmPtr A special pointer type wrapping the RuntimeAlgorithm
 *
 * @ingroup dynamic
 */
template <typename... Args>
RuntimeAlgorithmPtr eval(std::string const& name, Feature<Args> const&... args)
{
    return get_function_registry()[name].factory({args...});
}

} //namespace grunk
