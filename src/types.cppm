module;

#include <stdint.h>
#include <stddef.h>

export module types;

export using ::uint8_t;
export using ::int8_t;
export using ::uint16_t;
export using ::int16_t;
export using ::uint32_t;
export using ::int32_t;
export using ::uint64_t;
export using ::int64_t;
export using ::uintptr_t;
export using ::ptrdiff_t;
export using size_t = uint64_t;
export enum class byte : unsigned char {};
export using physaddr_t = uint64_t;
export using virtaddr_t = uint64_t;

export template <typename T>
struct span
{
public:
	using size_type = size_t;
	using reference = T&;
	using const_reference = const T&;
	using pointer = T*;
	using const_pointer = const T*;
	using iterator = pointer;
	using const_iterator = const_pointer;
	
	constexpr span() : m_data{nullptr}, m_size{0} {}
	constexpr span(const span&) = default;
	constexpr span(span&&) = default;
	
	constexpr span(pointer ptr, size_type sz) : m_data{ptr}, m_size{sz} {}
	
	constexpr span& operator=(const span&) = default;
	constexpr span& operator=(span&&) = default;

	constexpr iterator begin() const { return m_data; }
	constexpr const_iterator cbegin() const { return m_data; }

	constexpr iterator end() const { return m_data + m_size; }
	constexpr const_iterator cend() const { return m_data + m_size; }
	
	constexpr reference front() const { return m_data[0]; }
	constexpr reference back() const { return m_data[m_size - 1]; }

	constexpr reference operator[](size_type index) const { return m_data[index]; }
	constexpr pointer data() const { return m_data; }

	constexpr size_type size() const { return m_size; }
	constexpr size_type byte_size() const { return m_size * sizeof(T); }
	constexpr bool empty() const { return m_size == 0; }
private:
	T* m_data;
	size_t m_size;
};

export inline constexpr size_t string_length(const char* str)
{
	const char* begin = str;
	while(*str != '\0') {++str;}
	return static_cast<size_t>(str - begin);
}

export struct string_view
{
public:
	using char_type = char;
	using size_type = size_t;
	using reference = char_type&;
	using const_reference = const char_type&;
	using pointer = char_type*;
	using const_pointer = const char_type*;
	using iterator = pointer;
	using const_iterator = const_pointer;

	constexpr string_view() = delete;
	constexpr string_view(const string_view&) = default;
	constexpr string_view(string_view&&) = default;

	template <size_t N>
	constexpr string_view(const char_type (&str)[N]) : m_data{str}, m_size{N - 1} {}
	constexpr string_view(const_pointer str) : m_data{str}, m_size{string_length(str)} {}
	constexpr string_view(const_pointer str, const size_type str_size) : m_data{str}, m_size{str_size} {}
	constexpr string_view(const_iterator first, const_iterator last) : m_data{first}, m_size{static_cast<size_type>(last - first)} {}

	constexpr string_view& operator=(const string_view&) = default;
	constexpr string_view& operator=(string_view&&) = default;

	constexpr const_reference operator[](const size_type pos) const { return m_data[pos]; }
	constexpr const_reference front() const { return m_data[0]; }
	constexpr const_reference back() const { return m_data[m_size - 1]; }
	constexpr const_pointer data() const { return m_data; }

	constexpr const_iterator cbegin() const { return m_data; }
	constexpr const_iterator begin() const { return m_data; }

	constexpr const_iterator cend() const { return m_data + m_size; }
	constexpr const_iterator end() const { return m_data + m_size; }

	constexpr bool empty() const { return m_size == 0; }
	
	constexpr size_type size() const { return m_size; }
	constexpr size_type length() const { return m_size; }

	constexpr void remove_prefix(const size_type n)
	{
		m_data += n;
		m_size -= n;
	}

	constexpr void remove_suffix(const size_type n)
	{
		m_size -= n;
	}
private:
	const_pointer m_data{nullptr};
	size_type m_size{0};
};

export struct string_span
{
public:
	using char_type = char;
	using size_type = ptrdiff_t;
	using reference = char_type&;
	using const_reference = const char_type&;
	using pointer = char_type*;
	using const_pointer = const char_type*;
	using iterator = pointer;
	using const_iterator = const_pointer;

	constexpr string_span() = delete;
	constexpr string_span(const string_span&) = default;
	constexpr string_span(string_span&&) = default;

	template <size_t N>
	constexpr string_span(char_type (&str)[N]) : m_begin{str}, m_end{str + N - 1} {}
	constexpr string_span(pointer str) : m_begin{str}, m_end{str + string_length(str)} {}
	constexpr string_span(pointer str, const size_type sz) : m_begin{str}, m_end{str + sz} {}
	constexpr string_span(iterator first, iterator last) : m_begin{first}, m_end{last} {}

	constexpr string_span& operator=(const string_span&) = default;
	constexpr string_span& operator=(string_span&&) = default;

	constexpr reference operator[](const size_type index) { return m_begin[index]; }
	constexpr const_reference operator[](const size_type index) const { return m_begin[index]; }

	constexpr reference front() { return m_begin[0]; }
	constexpr const_reference front() const { return m_begin[0]; }

	constexpr reference back() { return *(m_end - 1); }
	constexpr const_reference back() const { return *(m_end - 1); }

	constexpr pointer data() { return m_begin; }
	constexpr const_pointer data() const { return m_begin; }

	constexpr iterator begin() { return m_begin; }
	constexpr const_iterator begin() const { return m_begin; }
	constexpr const_iterator cbegin() const { return m_begin; }

	constexpr iterator end() { return m_end; }
	constexpr const_iterator end() const { return m_end; }
	constexpr const_iterator cend() const { return m_end; }

	constexpr bool empty() const { return size() == 0; }
	constexpr size_type size() const { return static_cast<size_type>(m_end - m_begin); }
	constexpr size_type length() const { return static_cast<size_type>(m_end - m_begin); }

	constexpr void remove_prefix(const size_type n)
	{
		m_begin += n;
	}

	constexpr void remove_suffix(const size_type n)
	{
		m_end -= n;
	}
private:
	iterator m_begin{nullptr};
	iterator m_end{nullptr};
};



