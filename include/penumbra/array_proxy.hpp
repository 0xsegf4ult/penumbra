#pragma once

#include <penumbra/types.hpp>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace penumbra
{

template <typename T>
class array_proxy
{
public:
	constexpr array_proxy() noexcept : _data{nullptr}, _size{0} {}
	constexpr array_proxy(nullptr_t) noexcept : _data{nullptr}, _size{0} {}
	constexpr array_proxy(const T& value) noexcept : _data{&value}, _size{1} {}
	constexpr array_proxy(const T* ptr, size_t count) noexcept : _data{ptr}, _size{count} {}

	template <size_t arr_size>
	array_proxy(const T (&ptr)[arr_size] ) noexcept : _data{ptr}, _size{arr_size} {}

	array_proxy(const std::initializer_list<T>& list) noexcept : _data{list.begin()}, _size{list.size()} {}
	template <typename B = T, typename std::enable_if<std::is_const<B>::value, int>::type = 0>
	array_proxy(const std::initializer_list<typename std::remove_const<B>::type>& list) noexcept : _data{list.begin()}, _size{list.size()} {}

	template <typename V, typename std::enable_if<std::is_convertible<decltype(std::declval<V>().data()), T*>::value && std::is_convertible<decltype(std::declval<V>().size()), size_t>::value>::type * = nullptr>
	array_proxy(const V& v) noexcept : _data{v.data()}, _size{v.size()} {}

	const T* begin() const noexcept
	{
		return _data;
	}

	const T* end() const noexcept
	{
		return _data + _size;
	}

	const T& front() const noexcept
	{	
		return *_data;
	}

	const T& back() const noexcept
	{
		return *(_data + _size - 1);
	}

	const T& operator[](size_t index) const noexcept
	{
		return *(_data + index);
	}

	[[nodiscard]] bool empty() const noexcept
	{
		return _size == 0;
	}

	[[nodiscard]] size_t size() const noexcept
	{
		return _size;
	}

	const T* data() const noexcept
	{
		return _data;
	}
private:
	const T* _data;
	size_t _size;
};

}
