/**
 * @file RuntimeFunction.h
 *
 * This file contains the declaration and implementation of RuntimeFunctions
 */

#pragma once

#include <functional>
#include <vector>

#include <reflect/Reflect.hpp>

namespace grunk {


namespace details {

/**
 * @brief is_tuple_v returns false if the input type is not a tuple
 * 
 * @tparam typename any ol' type
 */
template<typename> constexpr bool is_tuple_v = false;

/**
 * @brief is_tuple_v returns true, if the input template argument is a tuple
 * 
 * @tparam Args the element types of the tuple
 */
template<typename... Args>
constexpr bool is_tuple_v<std::tuple<Args...>> = true;

/**
 * @brief num_values returns the number of elements if the input type is a
 * an std::tuple and 1 otherwise
 * 
 * @tparam T 
 * @return constexpr size_t 
 */
template <typename T>
constexpr size_t num_values() {
    if constexpr (details::is_tuple_v<T>) {
        return std::tuple_size_v<T>;
    }
    else {
        return 1;
    }
}

// function_traits gets the argument types of a callable object
// as seen on http://functionalcpp.wordpress.com/2013/08/05/function-traits/

/**
 * @brief A meta-programming function to query information on function types, 
 * e.g. the number and types of arguments and outputs
 * 
 * @tparam F 
 */
template<class F>
struct function_traits;

/**
 * @brief template specialization of function_traits for function pointers
 * 
 * @tparam R return type
 * @tparam Args argument types
 */
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};

/**
 * @brief template specialization of function_traits for functions
 * 
 * @tparam R return type
 * @tparam Args arguments
 */
template<class R, class... Args>
struct function_traits<R(Args...)>
{
    /**
     * @brief The return type of the function
     */
    using return_type = R;

    /**
     * @brief The number of input arguments
     */
    static constexpr std::size_t arity = sizeof...(Args);

    /**
     * @brief represents the ith argument of the function
     * 
     * @tparam N 
     */
    template <std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    };
};

/**
 * @brief template specialization of function_traits for member function pointers
 * 
 * @tparam C The parent class
 * @tparam R The return type
 * @tparam Args The arguments of the member function
 */
template<class C, class R, class... Args>
struct function_traits<R(C::*)(Args...)> : public function_traits<R(C&,Args...)>
{};

/**
 * @brief template specialization of function_traits for const member function pointers
 * 
 * @tparam C The parent class
 * @tparam R The return type
 * @tparam Args The arguments of the member function
 */
template<class C, class R, class... Args>
struct function_traits<R(C::*)(Args...) const> : public function_traits<R(C&,Args...)>
{};

/**
 * @brief template specialization of function_traits for data member pointers
 * 
 * @tparam C The parent class
 * @tparam R The type of the data member
 */
template<class C, class R>
struct function_traits<R(C::*)> : public function_traits<R(C&)>
{};

// functor
/**
 * @brief template specialization of function_traits for function objects,
 * that is classes implementing a call operator.
 * 
 * @tparam F The type of the function/the class
 */
template<class F>
struct function_traits
{
    private:
        /**
         * @brief the return type of the call operator
         */
        using call_type = function_traits<decltype(&F::operator())>;
    public:

        /**
         * @brief The return type of the function object
         * 
         */
        using return_type = typename call_type::return_type;

        /**
         * @brief the number of input arguments
         */
        static constexpr std::size_t arity = call_type::arity - 1;

        /**
         * @brief represents the type of the Nth argument
         * 
         * @tparam N the index of the argument
         */
        template <std::size_t N>
        struct argument
        {
            static_assert(N < arity, "error: invalid parameter index.");
            using type = typename call_type::template argument<N+1>::type;
        };
};

/**
 * @brief template specialization of function_traits for lvalue reference,
 * 
 * @tparam F The type of the function
 */
template<class F>
struct function_traits<F&> : public function_traits<F>
{};

/**
 * @brief template specialization of function_traits for rvalue reference,
 * 
 * @tparam F The type of the function
 */
template<class F>
struct function_traits<F&&> : public function_traits<F>
{};

} // namespace details


// turns any function into a function taking Reflect::DynamicObjects and returns a vector of Reflect::DynamicObjects.
// For "normal" functions the vector has 1 element. For void functions the vector is empty. For functions
// returning tuples, the vector has as many elements as the tuple.
/**
 * @brief Given any kind of function, this class represents a version of that function
 * that maps Reflect::DynamicObjects onto Reflect::DynamicObjects.
 *
 * It returns an std::vector<Reflect::DynamicObject>. For "normal" functions the vector 
 * has 1 element. For void functions the vector is empty. For functions returning 
 * tuples, the vector has as many elements as the tuple.
 *
 * @tparam The type of the input function
 * 
 * @ingroup dynamic_advanced
 */
template <typename F>
class RuntimeFunction
{

public:

    /**
     * @brief The return type of the wrapped function
     * 
     */
    using ResultType = typename details::function_traits<F>::return_type;

    /**
     * @brief The number of outputs of the wrapped function.
     * 
     * If ResultType is void, this is zero.
     * If ResultType is not an std::tuple, this is one.
     * If ResultType is an std::tuple, this is equal to the number of elements in the tuple.
     * 
     */
    static size_t const numOutputs = details::num_values<RuntimeFunction<F>::ResultType>();

    /**
     * @brief Construct a new RuntimeFunction object from any kind of function
     * 
     * @param f The input function
     */
    RuntimeFunction(F const& f)
     : fun{f}
    {}
    
    /**
     * @brief Evaluates the function given Reflect::DynamicObjects
     * 
     * @tparam Inputs Reflect::DynamicObjects to be passed to the wrapped function
     * @param inputs Reflect::DynamicObjects to be passed to the wrapped function
     * @return std::vector<Reflect::DynamicObject> The return value(s) of the wrapped function
     */
    template <typename... Inputs, typename Indices = std::make_index_sequence<sizeof...(Inputs)>>
    std::vector<Reflect::DynamicObject> operator()(Inputs&&... inputs) const
    {
       return call(Indices{}, std::forward<Inputs>(inputs)...);
    }
private:

    /**
     * @brief An internal helper function to call the wrapped function using
     * the indices trick. 
     * 
     * @tparam Inputs The Reflect::DynamicObjects on which the wrapped function shall be called
     * @tparam Is The indices of the arguments
     * @param inputs The Reflect::DynamicObjects on which the wrapped function shall be called
     * @return std::vector<Reflect::DynamicObject> The return value(s) of the wrapped function.
     */
    template <typename... Inputs, size_t... Is>
    std::vector<Reflect::DynamicObject> call(std::index_sequence<Is...>, Inputs&&... inputs) const
    {

        auto invoke = [this](Inputs&... i){
            return std::invoke(
                std::forward<F>(fun),
                Reflect::cast<typename details::function_traits<F>::template argument<Is>::type>(i)...
            ); 
        };

        if constexpr (std::is_void_v<ResultType>) {
            // if the function is void, cast arguments, call function ...
            invoke(inputs...);
            // ... and return empty vector
            return {};
        }
        else {
            // return type is not void. Evaluate the function and ...
            auto ret = invoke(inputs...);

            if constexpr (numOutputs > 1) {
                // ... if the return type is a tuple, store results in a vector of Reflect::DynamicObjects
                return std::apply([](auto&&... elems){
                    return std::vector<Reflect::DynamicObject>{Reflect::DynamicObject(std::move(elems))...};
                    }, 
                    std::forward<decltype(ret)>(ret)
                );
            }
            else {
                // ... if the return type is not a tuple, store the result in a one element vector
                return std::vector<Reflect::DynamicObject>(1, Reflect::DynamicObject(std::move(ret)));
            }
        }
    }

    F mutable fun;
};

} // namespace grunk
