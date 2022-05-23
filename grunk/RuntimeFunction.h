#pragma once

#include <functional>
#include <vector>

#include "RuntimeObject.h"

namespace grunk {


namespace details {

// is_tuple_v checks if a type is a tuple

template<typename> constexpr bool is_tuple_v = false;

template<typename... Args>
constexpr bool is_tuple_v<std::tuple<Args...>> = true;

// arg_type gets the argument types of a callable object

template <typename> struct arg_types;


// arg_type of function pointer
template <typename R, typename... Args> 
struct arg_types<R(*)(Args...)> {
  using type = std::tuple<Args...>;
}; 

// arg_type of function
template <typename R, typename... Args> 
struct arg_types<R(Args...)> {
  using type = std::tuple<Args...>;
}; 

// arg_type of const member function
template <typename R, typename T, typename... Args> 
struct arg_types<R(T::*)(Args...) const> {
  using type = std::tuple<T const&, Args...>;
};

// arg_type of non-const member function
template <typename R, typename T, typename... Args> 
struct arg_types<R(T::*)(Args...)> {
  using type = std::tuple<T&, Args...>;
};

template <typename T>
using arg_types_t = typename arg_types<T>::type;

} // namespace details


// turns any function into a function taking RuntimeObjects and returns a vector of RuntimeObjects.
// For "normal" functions the vector has 1 element. For void functions the vector is empty. For functions
// returning tuples, the vector has as many elements as the tuple.
template <typename F>
class RuntimeFunction
{

    using args = typename details::arg_types_t<F>;

public:
    RuntimeFunction(F const& f)
     : fun{f}
    {}
    
    template <typename... Inputs, typename Indices = std::make_index_sequence<sizeof...(Inputs)>>
    std::vector<RuntimeObject> operator()(Inputs&&... inputs) const
    {
       return call(Indices{}, std::forward<Inputs>(inputs)...);
    }
private:

    template <typename... Inputs, size_t... Is>
    std::vector<RuntimeObject> call(std::index_sequence<Is...>, Inputs&&... inputs) const
    {
        using ResultType = std::invoke_result_t<F, typename std::tuple_element<Is, args>::type...>;

        auto invoke = [this](Inputs const&... i){
            return std::invoke(
                fun,
                i.template cast<typename std::tuple_element<Is, args>::type>()...
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

            if constexpr (details::is_tuple_v<decltype(ret)>) {
                // ... if the return type is a tuple, store results in a vector of RuntimeObjects
                return std::apply([](auto&&... elems){
                    return std::vector<RuntimeObject>{std::forward<decltype(elems)>(elems)...};
                    }, 
                    std::forward<decltype(ret)>(ret)
                );
            }
            else {
                // ... if the return type is not a tuple, store the result in a one element vector
                return std::vector<RuntimeObject>(1, ret);
            }
        }
    }

    F const fun;
};

} // namespace grunk