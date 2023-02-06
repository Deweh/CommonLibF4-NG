#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <exception>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <istream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <numbers>
#include <numeric>
#include <optional>
#include <random>
#include <regex>
#include <set>
#include <source_location>
#include <span>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

static_assert(
	std::is_integral_v<std::time_t> && sizeof(std::time_t) == sizeof(std::size_t),
	"wrap std::time_t instead");

#pragma warning(push, 0)
#include <binary_io/file_stream.hpp>
#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/stl_interfaces/sequence_container_interface.hpp>
#include <fmt/format.h>
#include <mmio/mmio.hpp>
#include <spdlog/spdlog.h>
#pragma warning(pop)

#include "F4SE/Impl/WinAPI.h"

namespace F4SE
{
	using namespace std::literals;

	namespace stl
	{
		template <class CharT>
		using basic_zstring = std::basic_string_view<CharT>;

		using zstring = basic_zstring<char>;
		using zwstring = basic_zstring<wchar_t>;

		// owning pointer
		template <
			class T,
			class = std::enable_if_t<
				std::is_pointer_v<T>>>
		using owner = T;

		// non-owning pointer
		template <
			class T,
			class = std::enable_if_t<
				std::is_pointer_v<T>>>
		using observer = T;

		// non-null pointer
		template <
			class T,
			class = std::enable_if_t<
				std::is_pointer_v<T>>>
		using not_null = T;

		template <class T>
		struct remove_cvptr
		{
			using type = std::remove_cv_t<std::remove_pointer_t<T>>;
		};

		template <class T>
		using remove_cvptr_t = typename remove_cvptr<T>::type;

		template <class C, class K>
		concept transparent_comparator =
			requires(
				const K& a_transparent,
				const typename C::key_type& a_key,
				typename C::key_compare& a_compare)
		{
			typename C::key_compare::is_transparent;
			// clang-format off
			{ a_compare(a_transparent, a_key) } -> std::convertible_to<bool>;
			{ a_compare(a_key, a_transparent) } -> std::convertible_to<bool>;
			// clang-format on
		};

		namespace nttp
		{
			template <class CharT, std::size_t N>
			struct string
			{
				using char_type = CharT;
				using pointer = char_type*;
				using const_pointer = const char_type*;
				using reference = char_type&;
				using const_reference = const char_type&;
				using size_type = std::size_t;

				static constexpr auto npos = static_cast<std::size_t>(-1);

				consteval string(const_pointer a_string) noexcept
				{
					for (size_type i = 0; i < N; ++i) {
						c[i] = a_string[i];
					}
				}

				[[nodiscard]] consteval const_reference operator[](size_type a_pos) const noexcept
				{
					assert(a_pos < N);
					return c[a_pos];
				}

				[[nodiscard]] consteval char_type value_at(size_type a_pos) const noexcept
				{
					assert(a_pos < N);
					return c[a_pos];
				}

				[[nodiscard]] consteval const_reference back() const noexcept { return (*this)[size() - 1]; }
				[[nodiscard]] consteval const_pointer data() const noexcept { return c; }
				[[nodiscard]] consteval bool empty() const noexcept { return this->size() == 0; }
				[[nodiscard]] consteval const_reference front() const noexcept { return (*this)[0]; }
				[[nodiscard]] consteval size_type length() const noexcept { return N; }
				[[nodiscard]] consteval size_type size() const noexcept { return length(); }

				template <std::size_t POS = 0, std::size_t COUNT = npos>
				[[nodiscard]] consteval auto substr() const noexcept
				{
					return string < CharT, COUNT != npos ? COUNT : N - POS > (this->data() + POS);
				}

				char_type c[N] = {};
			};

			template <class CharT, std::size_t N>
			string(const CharT (&)[N]) -> string<CharT, N - 1>;
		}

		template <class EF>                                        //
			requires(std::invocable<std::remove_reference_t<EF>>)  //
		class scope_exit
		{
		public:
			// 1)
			template <class Fn>
			explicit scope_exit(Fn&& a_fn)  //
				noexcept(std::is_nothrow_constructible_v<EF, Fn> ||
						 std::is_nothrow_constructible_v<EF, Fn&>)  //
				requires(!std::is_same_v<std::remove_cvref_t<Fn>, scope_exit> &&
						 std::is_constructible_v<EF, Fn>)
			{
				static_assert(std::invocable<Fn>);

				if constexpr (!std::is_lvalue_reference_v<Fn> &&
							  std::is_nothrow_constructible_v<EF, Fn>) {
					_fn.emplace(std::forward<Fn>(a_fn));
				} else {
					_fn.emplace(a_fn);
				}
			}

			// 2)
			scope_exit(scope_exit&& a_rhs)  //
				noexcept(std::is_nothrow_move_constructible_v<EF> ||
						 std::is_nothrow_copy_constructible_v<EF>)  //
				requires(std::is_nothrow_move_constructible_v<EF> ||
						 std::is_copy_constructible_v<EF>)
			{
				static_assert(!(std::is_nothrow_move_constructible_v<EF> && !std::is_move_constructible_v<EF>));
				static_assert(!(!std::is_nothrow_move_constructible_v<EF> && !std::is_copy_constructible_v<EF>));

				if (a_rhs.active()) {
					if constexpr (std::is_nothrow_move_constructible_v<EF>) {
						_fn.emplace(std::forward<EF>(*a_rhs._fn));
					} else {
						_fn.emplace(a_rhs._fn);
					}
					a_rhs.release();
				}
			}

			// 3)
			scope_exit(const scope_exit&) = delete;

			~scope_exit() noexcept
			{
				if (_fn.has_value()) {
					(*_fn)();
				}
			}

			void release() noexcept { _fn.reset(); }

		private:
			[[nodiscard]] bool active() const noexcept { return _fn.has_value(); }

			std::optional<std::remove_reference_t<EF>> _fn;
		};

		template <class EF>
		scope_exit(EF) -> scope_exit<EF>;

		template <class F>
		class counted_function_iterator :
			public boost::stl_interfaces::iterator_interface<
				counted_function_iterator<F>,
				std::input_iterator_tag,
				std::remove_reference_t<decltype(std::declval<F>()())>>
		{
		private:
			using super =
				boost::stl_interfaces::iterator_interface<
					counted_function_iterator<F>,
					std::input_iterator_tag,
					std::remove_reference_t<decltype(std::declval<F>()())>>;

		public:
			using difference_type = typename super::difference_type;
			using value_type = typename super::value_type;
			using pointer = typename super::pointer;
			using reference = typename super::reference;
			using iterator_category = typename super::iterator_category;

			counted_function_iterator() noexcept = default;

			counted_function_iterator(
				F a_fn,
				std::size_t a_count) noexcept :
				_fn(std::move(a_fn)),
				_left(a_count)
			{}

			[[nodiscard]] reference operator*() const  //
				noexcept(noexcept(std::declval<F>()()))
			{
				assert(_fn != std::nullopt);
				return (*_fn)();
			}

			[[nodiscard]] friend bool operator==(
				const counted_function_iterator& a_lhs,
				const counted_function_iterator& a_rhs) noexcept
			{
				return a_lhs._left == a_rhs._left;
			}

			using super::operator++;

			void operator++() noexcept
			{
				assert(_left > 0);
				_left -= 1;
			}

		private:
			std::optional<F> _fn;
			std::size_t _left{ 0 };
		};

		template <
			class Enum,
			class Underlying = std::underlying_type_t<Enum>>
		class enumeration
		{
		public:
			using enum_type = Enum;
			using underlying_type = Underlying;

			static_assert(std::is_enum_v<enum_type>, "enum_type must be an enum");
			static_assert(std::is_integral_v<underlying_type>, "underlying_type must be an integral");

			constexpr enumeration() noexcept = default;

			constexpr enumeration(const enumeration&) noexcept = default;

			constexpr enumeration(enumeration&&) noexcept = default;

			template <class U2>  // NOLINTNEXTLINE(google-explicit-constructor)
			constexpr enumeration(enumeration<Enum, U2> a_rhs) noexcept :
				_impl(static_cast<underlying_type>(a_rhs.get()))
			{}

			template <class... Args>
			constexpr enumeration(Args... a_values) noexcept  //
				requires(std::same_as<Args, enum_type> && ...)
				:
				_impl((static_cast<underlying_type>(a_values) | ...))
			{}

			~enumeration() noexcept = default;

			constexpr enumeration& operator=(const enumeration&) noexcept = default;
			constexpr enumeration& operator=(enumeration&&) noexcept = default;

			template <class U2>
			constexpr enumeration& operator=(enumeration<Enum, U2> a_rhs) noexcept
			{
				_impl = static_cast<underlying_type>(a_rhs.get());
			}

			constexpr enumeration& operator=(enum_type a_value) noexcept
			{
				_impl = static_cast<underlying_type>(a_value);
				return *this;
			}

			[[nodiscard]] explicit constexpr operator bool() const noexcept { return _impl != static_cast<underlying_type>(0); }

			[[nodiscard]] constexpr enum_type operator*() const noexcept { return get(); }
			[[nodiscard]] constexpr enum_type get() const noexcept { return static_cast<enum_type>(_impl); }
			[[nodiscard]] constexpr underlying_type underlying() const noexcept { return _impl; }

			template <class... Args>
			constexpr enumeration& set(Args... a_args) noexcept  //
				requires(std::same_as<Args, enum_type> && ...)
			{
				_impl |= (static_cast<underlying_type>(a_args) | ...);
				return *this;
			}

			template <class... Args>
			constexpr enumeration& reset(Args... a_args) noexcept  //
				requires(std::same_as<Args, enum_type> && ...)
			{
				_impl &= ~(static_cast<underlying_type>(a_args) | ...);
				return *this;
			}

			template <class... Args>
			[[nodiscard]] constexpr bool any(Args... a_args) const noexcept  //
				requires(std::same_as<Args, enum_type> && ...)
			{
				return (_impl & (static_cast<underlying_type>(a_args) | ...)) != static_cast<underlying_type>(0);
			}

			template <class... Args>
			[[nodiscard]] constexpr bool all(Args... a_args) const noexcept  //
				requires(std::same_as<Args, enum_type> && ...)
			{
				return (_impl & (static_cast<underlying_type>(a_args) | ...)) == (static_cast<underlying_type>(a_args) | ...);
			}

			template <class... Args>
			[[nodiscard]] constexpr bool none(Args... a_args) const noexcept  //
				requires(std::same_as<Args, enum_type> && ...)
			{
				return (_impl & (static_cast<underlying_type>(a_args) | ...)) == static_cast<underlying_type>(0);
			}

		private:
			underlying_type _impl{ 0 };
		};

		template <class... Args>
		enumeration(Args...) -> enumeration<
			std::common_type_t<Args...>,
			std::underlying_type_t<
				std::common_type_t<Args...>>>;
	}
}

#define F4SE_MAKE_LOGICAL_OP(a_op, a_result)                                                                    \
	template <class E, class U1, class U2>                                                                      \
	[[nodiscard]] constexpr a_result operator a_op(enumeration<E, U1> a_lhs, enumeration<E, U2> a_rhs) noexcept \
	{                                                                                                           \
		return a_lhs.get() a_op a_rhs.get();                                                                    \
	}                                                                                                           \
                                                                                                                \
	template <class E, class U>                                                                                 \
	[[nodiscard]] constexpr a_result operator a_op(enumeration<E, U> a_lhs, E a_rhs) noexcept                   \
	{                                                                                                           \
		return a_lhs.get() a_op a_rhs;                                                                          \
	}

#define F4SE_MAKE_ARITHMETIC_OP(a_op)                                                        \
	template <class E, class U>                                                              \
	[[nodiscard]] constexpr auto operator a_op(enumeration<E, U> a_enum, U a_shift) noexcept \
		-> enumeration<E, U>                                                                 \
	{                                                                                        \
		return static_cast<E>(static_cast<U>(a_enum.get()) a_op a_shift);                    \
	}                                                                                        \
                                                                                             \
	template <class E, class U>                                                              \
	constexpr auto operator a_op##=(enumeration<E, U>& a_enum, U a_shift) noexcept           \
		-> enumeration<E, U>&                                                                \
	{                                                                                        \
		return a_enum = a_enum a_op a_shift;                                                 \
	}

#define F4SE_MAKE_ENUMERATION_OP(a_op)                                                                      \
	template <class E, class U1, class U2>                                                                  \
	[[nodiscard]] constexpr auto operator a_op(enumeration<E, U1> a_lhs, enumeration<E, U2> a_rhs) noexcept \
		-> enumeration<E, std::common_type_t<U1, U2>>                                                       \
	{                                                                                                       \
		return static_cast<E>(static_cast<U1>(a_lhs.get()) a_op static_cast<U2>(a_rhs.get()));              \
	}                                                                                                       \
                                                                                                            \
	template <class E, class U>                                                                             \
	[[nodiscard]] constexpr auto operator a_op(enumeration<E, U> a_lhs, E a_rhs) noexcept                   \
		-> enumeration<E, U>                                                                                \
	{                                                                                                       \
		return static_cast<E>(static_cast<U>(a_lhs.get()) a_op static_cast<U>(a_rhs));                      \
	}                                                                                                       \
                                                                                                            \
	template <class E, class U>                                                                             \
	[[nodiscard]] constexpr auto operator a_op(E a_lhs, enumeration<E, U> a_rhs) noexcept                   \
		-> enumeration<E, U>                                                                                \
	{                                                                                                       \
		return static_cast<E>(static_cast<U>(a_lhs) a_op static_cast<U>(a_rhs.get()));                      \
	}                                                                                                       \
                                                                                                            \
	template <class E, class U1, class U2>                                                                  \
	constexpr auto operator a_op##=(enumeration<E, U1>& a_lhs, enumeration<E, U2> a_rhs) noexcept           \
		-> enumeration<E, U1>&                                                                              \
	{                                                                                                       \
		return a_lhs = a_lhs a_op a_rhs;                                                                    \
	}                                                                                                       \
                                                                                                            \
	template <class E, class U>                                                                             \
	constexpr auto operator a_op##=(enumeration<E, U>& a_lhs, E a_rhs) noexcept                             \
		-> enumeration<E, U>&                                                                               \
	{                                                                                                       \
		return a_lhs = a_lhs a_op a_rhs;                                                                    \
	}                                                                                                       \
                                                                                                            \
	template <class E, class U>                                                                             \
	constexpr auto operator a_op##=(E& a_lhs, enumeration<E, U> a_rhs) noexcept                             \
		-> E&                                                                                               \
	{                                                                                                       \
		return a_lhs = *(a_lhs a_op a_rhs);                                                                 \
	}

#define F4SE_MAKE_INCREMENTER_OP(a_op)                                                       \
	template <class E, class U>                                                              \
	constexpr auto operator a_op##a_op(enumeration<E, U>& a_lhs) noexcept                    \
		-> enumeration<E, U>&                                                                \
	{                                                                                        \
		return a_lhs a_op## = static_cast<E>(1);                                             \
	}                                                                                        \
                                                                                             \
	template <class E, class U>                                                              \
	[[nodiscard]] constexpr auto operator a_op##a_op(enumeration<E, U>& a_lhs, int) noexcept \
		-> enumeration<E, U>                                                                 \
	{                                                                                        \
		const auto tmp = a_lhs;                                                              \
		a_op##a_op a_lhs;                                                                    \
		return tmp;                                                                          \
	}

namespace F4SE
{
	namespace stl
	{
		template <
			class E,
			class U>
		[[nodiscard]] constexpr auto operator~(enumeration<E, U> a_enum) noexcept
			-> enumeration<E, U>
		{
			return static_cast<E>(~static_cast<U>(a_enum.get()));
		}

		F4SE_MAKE_LOGICAL_OP(==, bool);
		F4SE_MAKE_LOGICAL_OP(<=>, std::strong_ordering);

		F4SE_MAKE_ARITHMETIC_OP(<<);
		F4SE_MAKE_ENUMERATION_OP(<<);
		F4SE_MAKE_ARITHMETIC_OP(>>);
		F4SE_MAKE_ENUMERATION_OP(>>);

		F4SE_MAKE_ENUMERATION_OP(|);
		F4SE_MAKE_ENUMERATION_OP(&);
		F4SE_MAKE_ENUMERATION_OP(^);

		F4SE_MAKE_ENUMERATION_OP(+);
		F4SE_MAKE_ENUMERATION_OP(-);

		F4SE_MAKE_INCREMENTER_OP(+);  // ++
		F4SE_MAKE_INCREMENTER_OP(-);  // --

		template <class T>
		class atomic_ref :
			public std::atomic_ref<T>
		{
		private:
			using super = std::atomic_ref<T>;

		public:
			using value_type = typename super::value_type;

			explicit atomic_ref(volatile T& a_obj) noexcept(std::is_nothrow_constructible_v<super, value_type&>) :
				super(const_cast<value_type&>(a_obj))
			{}

			using super::super;
			using super::operator=;
		};

		template <class T>
		atomic_ref(volatile T&) -> atomic_ref<T>;

		template class atomic_ref<std::int8_t>;
		template class atomic_ref<std::uint8_t>;
		template class atomic_ref<std::int16_t>;
		template class atomic_ref<std::uint16_t>;
		template class atomic_ref<std::int32_t>;
		template class atomic_ref<std::uint32_t>;
		template class atomic_ref<std::int64_t>;
		template class atomic_ref<std::uint64_t>;

		static_assert(atomic_ref<std::int8_t>::is_always_lock_free);
		static_assert(atomic_ref<std::uint8_t>::is_always_lock_free);
		static_assert(atomic_ref<std::int16_t>::is_always_lock_free);
		static_assert(atomic_ref<std::uint16_t>::is_always_lock_free);
		static_assert(atomic_ref<std::int32_t>::is_always_lock_free);
		static_assert(atomic_ref<std::uint32_t>::is_always_lock_free);
		static_assert(atomic_ref<std::int64_t>::is_always_lock_free);
		static_assert(atomic_ref<std::uint64_t>::is_always_lock_free);

		template <class T>
		struct ssizeof
		{
			[[nodiscard]] constexpr operator std::ptrdiff_t() const noexcept { return value; }

			[[nodiscard]] constexpr std::ptrdiff_t operator()() const noexcept { return value; }

			static constexpr auto value = static_cast<std::ptrdiff_t>(sizeof(T));
		};

		template <class T>
		inline constexpr auto ssizeof_v = ssizeof<T>::value;

		template <class T, class U>
		[[nodiscard]] auto adjust_pointer(U* a_ptr, std::ptrdiff_t a_adjust) noexcept
		{
			auto addr = a_ptr ? reinterpret_cast<std::uintptr_t>(a_ptr) + a_adjust : 0;
			if constexpr (std::is_const_v<U> && std::is_volatile_v<U>) {
				return reinterpret_cast<std::add_cv_t<T>*>(addr);
			} else if constexpr (std::is_const_v<U>) {
				return reinterpret_cast<std::add_const_t<T>*>(addr);
			} else if constexpr (std::is_volatile_v<U>) {
				return reinterpret_cast<std::add_volatile_t<T>*>(addr);
			} else {
				return reinterpret_cast<T*>(addr);
			}
		}

		template <class T>
		bool emplace_vtable(T* a_ptr)
		{
			auto address = T::VTABLE[0].address();
			if (!address) {
				return false;
			}
			reinterpret_cast<std::uintptr_t*>(a_ptr)[0] = address;
			return true;
		}

		template <class T>
		void memzero(volatile T* a_ptr, std::size_t a_size = sizeof(T))
		{
			const auto begin = reinterpret_cast<volatile char*>(a_ptr);
			constexpr char val{ 0 };
			std::fill_n(begin, a_size, val);
		}

		template <class... Args>
		[[nodiscard]] inline auto pun_bits(Args... a_args)  //
			requires(std::same_as<std::remove_cv_t<Args>, bool> && ...)
		{
			constexpr auto ARGC = sizeof...(Args);

			std::bitset<ARGC> bits;
			std::size_t i = 0;
			((bits[i++] = a_args), ...);

			if constexpr (ARGC <= std::numeric_limits<unsigned long>::digits) {
				return bits.to_ulong();
			} else if constexpr (ARGC <= std::numeric_limits<unsigned long long>::digits) {
				return bits.to_ullong();
			} else {
				static_assert(false && sizeof...(Args));
			}
		}

		[[nodiscard]] inline auto utf8_to_utf16(std::string_view a_in) noexcept
			-> std::optional<std::wstring>
		{
			const auto cvt = [&](wchar_t* a_dst, std::size_t a_length) {
				return WinAPI::MultiByteToWideChar(
					CP_UTF8,
					0,
					a_in.data(),
					static_cast<int>(a_in.length()),
					a_dst,
					static_cast<int>(a_length));
			};

			const auto len = cvt(nullptr, 0);
			if (len == 0) {
				return std::nullopt;
			}

			std::wstring out(len, '\0');
			if (cvt(out.data(), out.length()) == 0) {
				return std::nullopt;
			}

			return out;
		}

		[[nodiscard]] inline auto utf16_to_utf8(std::wstring_view a_in) noexcept
			-> std::optional<std::string>
		{
			const auto cvt = [&](char* a_dst, std::size_t a_length) {
				return WinAPI::WideCharToMultiByte(
					CP_UTF8,
					0,
					a_in.data(),
					static_cast<int>(a_in.length()),
					a_dst,
					static_cast<int>(a_length),
					nullptr,
					nullptr);
			};

			const auto len = cvt(nullptr, 0);
			if (len == 0) {
				return std::nullopt;
			}

			std::string out(len, '\0');
			if (cvt(out.data(), out.length()) == 0) {
				return std::nullopt;
			}

			return out;
		}

#ifndef __clang__
		using source_location = std::source_location;
#else
		/**
		 * A polyfill for source location support for Clang.
		 *
		 * <p>
		 * Clang-CL can use <code>source_location</code>, but not in the context of a default argument due to
		 * a bug in its support for <code>consteval</code>. This bug does not affect <code>constexpr</code> so
		 * this class uses a <code>constexpr</code> version of the typical constructor.
		 * </p>
		 */
		struct source_location
		{
		public:
			static constexpr source_location current(
				const uint_least32_t a_line = __builtin_LINE(),
				const uint_least32_t a_column = __builtin_COLUMN(),
				const char* const a_file = __builtin_FILE(),
				const char* const a_function = __builtin_FUNCTION()) noexcept
			{
				source_location result;
				result._line = a_line;
				result._column = a_column;
				result._file = a_file;
				result._function = a_function;
				return result;
			}

			[[nodiscard]] constexpr const char* file_name() const noexcept
			{
				return _file;
			}

			[[nodiscard]] constexpr const char* function_name() const noexcept
			{
				return _function;
			}

			[[nodiscard]] constexpr uint_least32_t line() const noexcept
			{
				return _line;
			}

			[[nodiscard]] constexpr uint_least32_t column() const noexcept
			{
				return _column;
			}

		private:
			source_location() = default;

			uint_least32_t _line{};
			uint_least32_t _column{};
			const char* _file = "";
			const char* _function = "";
		};
#endif

		inline bool report_and_error(std::string_view a_msg, bool a_fail = true,
			F4SE::stl::source_location a_loc = F4SE::stl::source_location::current())
		{
			const auto body = [&]() -> std::wstring {
				const std::filesystem::path p = a_loc.file_name();
				auto filename = p.lexically_normal().generic_string();

				const std::regex r{ R"((?:^|[\\\/])(?:include|src)[\\\/](.*)$)" };
				std::smatch matches;
				if (std::regex_search(filename, matches, r)) {
					filename = matches[1].str();
				}

				return utf8_to_utf16(
					fmt::format(
						"{}({}): {}"sv,
						filename,
						a_loc.line(),
						a_msg))
				    .value_or(L"<character encoding error>"s);
			}();

			const auto caption = []() {
				const auto maxPath = WinAPI::GetMaxPath();
				std::vector<wchar_t> buf;
				buf.reserve(maxPath);
				buf.resize(maxPath / 2);
				std::uint32_t result = 0;
				do {
					buf.resize(buf.size() * 2);
					result = GetModuleFileName(
						WinAPI::GetCurrentModule(),
						buf.data(),
						static_cast<std::uint32_t>(buf.size()));
				} while (result && result == buf.size() && buf.size() <= (std::numeric_limits<std::uint32_t>::max)());

				if (result && result != buf.size()) {
					std::filesystem::path p(buf.begin(), buf.begin() + result);
					return p.filename().native();
				} else {
					return L""s;
				}
			}();

			spdlog::log(
				spdlog::source_loc{
					a_loc.file_name(),
					static_cast<int>(a_loc.line()),
					a_loc.function_name() },
				spdlog::level::critical,
				a_msg);

			if (a_fail) {
#ifdef ENABLE_COMMONLIBF4_TESTING
				throw std::runtime_error(utf16_to_utf8(caption.empty() ? body.c_str() : caption.c_str())->c_str());
#else
				MessageBox(nullptr, body.c_str(), (caption.empty() ? nullptr : caption.c_str()), 0);
				WinAPI::TerminateProcess(WinAPI::GetCurrentProcess(), EXIT_FAILURE);
#endif
			}
			return true;
		}

		[[noreturn]] inline void report_and_fail(std::string_view a_msg,
			F4SE::stl::source_location a_loc = F4SE::stl::source_location::current())
		{
			report_and_error(a_msg, true, a_loc);
		}

		template <class Enum>
		[[nodiscard]] constexpr auto to_underlying(Enum a_val) noexcept  //
			requires(std::is_enum_v<Enum>)
		{
			return static_cast<std::underlying_type_t<Enum>>(a_val);
		}

		template <class To, class From>
		[[nodiscard]] To unrestricted_cast(From a_from) noexcept
		{
			if constexpr (std::is_same_v<
							  std::remove_cv_t<From>,
							  std::remove_cv_t<To>>) {
				return To{ a_from };

				// From != To
			} else if constexpr (std::is_reference_v<From>) {
				return stl::unrestricted_cast<To>(std::addressof(a_from));

				// From: NOT reference
			} else if constexpr (std::is_reference_v<To>) {
				return *stl::unrestricted_cast<
					std::add_pointer_t<
						std::remove_reference_t<To>>>(a_from);

				// To: NOT reference
			} else if constexpr (std::is_pointer_v<From> &&
								 std::is_pointer_v<To>) {
				return static_cast<To>(
					const_cast<void*>(
						static_cast<const volatile void*>(a_from)));
			} else if constexpr ((std::is_pointer_v<From> && std::is_integral_v<To>) ||
								 (std::is_integral_v<From> && std::is_pointer_v<To>)) {
				return reinterpret_cast<To>(a_from);
			} else {
				union
				{
					std::remove_cv_t<std::remove_reference_t<From>> from;
					std::remove_cv_t<std::remove_reference_t<To>> to;
				};

				from = std::forward<From>(a_from);
				return to;
			}
		}
	}
}

#undef F4SE_MAKE_INCREMENTER_OP
#undef F4SE_MAKE_ENUMERATION_OP
#undef F4SE_MAKE_ARITHMETIC_OP
#undef F4SE_MAKE_LOGICAL_OP

namespace RE
{
	using namespace std::literals;
	namespace stl = F4SE::stl;
	namespace WinAPI = F4SE::WinAPI;
}

namespace REL
{
	using namespace std::literals;
	namespace stl = F4SE::stl;
	namespace WinAPI = F4SE::WinAPI;
}

#include "REL/Relocation.h"

#include "RE/NiRTTI_IDs.h"
#include "RE/RTTI_IDs.h"
#include "RE/VTABLE_IDs.h"

#include "RE/msvc/functional.h"
#include "RE/msvc/memory.h"
#include "RE/msvc/typeinfo.h"

#undef cdecl  // Workaround for Clang.
