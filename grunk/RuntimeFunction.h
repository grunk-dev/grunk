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
template<class F>
struct function_traits;

// function pointer
template<class R, class... Args>
struct function_traits<R(*)(Args...)> : public function_traits<R(Args...)>
{};

template<class R, class... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;

    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N,std::tuple<Args...>>::type;
    };
};

// member function pointer
template<class C, class R, class... Args>
struct function_traits<R(C::*)(Args...)> : public function_traits<R(C&,Args...)>
{};

// const member function pointer
template<class C, class R, class... Args>
struct function_traits<R(C::*)(Args...) const> : public function_traits<R(C&,Args...)>
{};

// member object pointer
template<class C, class R>
struct function_traits<R(C::*)> : public function_traits<R(C&)>
{};

// functor
template<class F>
struct function_traits
{
    private:
        using call_type = function_traits<decltype(&F::operator())>;
    public:
        using return_type = typename call_type::return_type;

        static constexpr std::size_t arity = call_type::arity - 1;

        template <std::size_t N>
        struct argument
        {
            static_assert(N < arity, "error: invalid parameter index.");
            using type = typename call_type::template argument<N+1>::type;
        };
};

template<class F>
struct function_traits<F&> : public function_traits<F>
{};

template<class F>
struct function_traits<F&&> : public function_traits<F>
{};

} // namespace details


// turns any function into a function taking RuntimeObjects and returns a vector of RuntimeObjects.
// For "normal" functions the vector has 1 element. For void functions the vector is empty. For functions
// returning tuples, the vector has as many elements as the tuple.
template <typename F>
class RuntimeFunction
{

public:

    using ResultType = typename details::function_traits<F>::return_type;

    static size_t const numOutputs = details::num_values<RuntimeFunction<F>::ResultType>();

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

        auto invoke = [this](Inputs&... i){
            return std::invoke(
                std::forward<F>(fun),
                i.template cast<typename details::function_traits<F>::template argument<Is>::type>()...
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

    F mutable fun;
};

} // namespace grunk