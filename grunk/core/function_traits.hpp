// SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
//
// SPDX-License-Identifier: MPL-2.0

#pragma once 

#include <tuple>

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
			else if constexpr (std::is_void_v<T>) {
				return 0;
			} else {
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

		template<class R, class... Args>
		struct function_traits<R(* const)(Args...)> : public function_traits<R(Args...)>
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

			/// @brief The arguments of the member function
			using arguments_tuple = std::tuple<Args...>;

			/**
			* @brief The number of input arguments
			*/
			static constexpr std::size_t arity = sizeof...(Args);

			/**
			* @brief represents the Nth argument of the function
			* 
			* @tparam N The index of the argument
			*/
			template <std::size_t N>
			struct argument
			{
				static_assert(N < arity, "error: invalid parameter index.");

				/// @brief the type of the Nth argument
				using type = typename std::tuple_element<N,arguments_tuple>::type;
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

		template<class C, class R, class... Args>
		struct function_traits<R(C::* const)(Args...)> : public function_traits<R(C const&,Args...)>
		{};

		/**
		* @brief template specialization of function_traits for const member function pointers
		* 
		* @tparam C The parent class
		* @tparam R The return type
		* @tparam Args The arguments of the member function
		*/
		template<class C, class R, class... Args>
		struct function_traits<R(C::*)(Args...) const> : public function_traits<R(C const&,Args...)>
		{};

		/**
		* @brief template specialization of function_traits for const member function pointers
		* 
		* @tparam C The parent class
		* @tparam R The return type
		* @tparam Args The arguments of the member function
		*/
		template<class C, class R, class... Args>
		struct function_traits<R(C::* const)(Args...) const> : public function_traits<R(C const&,Args...)>
		{};

		/**
		* @brief template specialization of function_traits for data member pointers
		* 
		* @tparam C The parent class
		* @tparam R The type of the data member
		*/
        template<class C, class R>
        struct function_traits<R(C::*)> : public function_traits<R&(C&)>
        {};

		/**
		 * @brief template specialization of function_traits for const data member pointers
		 * 
		 * @tparam C The parent class
		 * @tparam R The type of the data member
		 */
        template<class C, class R>
        struct function_traits<R const (C::*)> : public function_traits<R const& (C const&)>
        {};

		/**
		 * @brief A meta-programming function to query the tail of a tuple, stripping the first element type
		 * 
		 * @tparam T the tuple type
		 */
		template <typename T>
		struct tail_tuple;

		/**
		 * @brief template specialization of tail_tuple for std::tuple
		 * 
		 * @tparam Arg The first element type
		 * @tparam Args The remaining element types
		 */
		template <typename Arg, typename... Args>
		struct tail_tuple<std::tuple<Arg, Args...>> {
			/// @brief the actual tail of the tuple, without the first element type
			using type = std::tuple<Args...>;
		};

		template <typename T>
		using tail_tuple_t = typename tail_tuple<T>::type;

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

				/// @brief a tuple containing the arguments, with a stripped implicit this argument, if this is a member function
				using arguments_tuple = tail_tuple_t<typename call_type::arguments_tuple>;

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

					/// @brief the type of the Nth argument
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

} // namespace reflect
