export module kfmt;
import types;

void __assertion_fail_handler(const char*);

template <typename T>
constexpr int count_digits_hex(T n)
{
	int digits = 1;
	while((n >>= 4u) != 0)
		++digits;
	return digits;
}

constexpr const char* digits_hex_uppercase = "0123456789ABCDEF";
constexpr const char* digits_hex_lowercase = "0123456789abcdef";

template <typename T>
constexpr void convert_hex(char* dst, T value, const bool uppercase)
{
	const char* digits = uppercase ? digits_hex_uppercase : digits_hex_lowercase;
	
	do
	{
		const T v = value;
		value >>= 4u;
		*(--dst) = digits[v - (value << 4u)];
	} while(value);
}

struct fmt_arg_format
{
public:
	using iterator = char*;
	using const_iterator = const char*;

	enum class Type : uint8_t
	{
		None,
		Char,
		Integer,
		Pointer,
		Float,
		String
	};

	constexpr fmt_arg_format(string_view& fmt, const int arg_count)
	{
		const_iterator it = fmt.cbegin();

		if(*it != '{')
		{
			__assertion_fail_handler("failed to parse format string: expected character {");
			return;
		}

		++it;

		if(!(it < fmt.cend() && *it++ == '}'))
		{
			__assertion_fail_handler("failed to parse format string: unterminated format argument specifier");
			return;
		}

		fmt.remove_prefix(it - fmt.cbegin());
	}

	constexpr int index() const { return static_cast<int>(m_index); }

	void format_pointer(iterator& it, const_iterator end, const uintptr_t value)
	{
		auto ivalue = static_cast<uint64_t>(value);
		const auto digits = count_digits_hex(ivalue);
		*it++ = '0';
		*it++ = 'x';
		it += digits;
		convert_hex<uint64_t>(it, ivalue, false);
	}

	void format_string(iterator& it, const_iterator end, const string_view& str)
	{
		auto count = str.size();
		const auto* data = str.data();
		while((count--) > 0)
			*it++ = *data++;
	}
private:
	int8_t m_index{-1};
};


struct fmt_argument
{
public:
	using iterator = char*;
	using const_iterator = const char*;

	using Format = fmt_arg_format;

	constexpr fmt_argument() = delete;
	constexpr fmt_argument(const bool value) : v_bool{value}, type_id{TypeID::Bool} {}
	constexpr fmt_argument(const char value) : v_char{value}, type_id{TypeID::Char} {}
	constexpr fmt_argument(const int32_t value) : v_int32{value}, type_id{TypeID::Int32} {}
	constexpr fmt_argument(const uint32_t value) : v_uint32{value}, type_id{TypeID::UInt32} {}
	constexpr fmt_argument(const int64_t value) : v_int64{value}, type_id{TypeID::Int64} {}
	constexpr fmt_argument(const uint64_t value) : v_uint64{value}, type_id{TypeID::UInt64} {}
	constexpr fmt_argument(const void* value) : v_pointer{reinterpret_cast<uintptr_t>(value)}, type_id{TypeID::Pointer} {}
	constexpr fmt_argument(const string_view value) : v_string{value}, type_id{TypeID::String} {}

	constexpr void format(string_span& dst, Format& format) const
	{
		iterator it = dst.begin();

		switch(type_id)
		{
		case TypeID::Pointer: 
			format.format_pointer(it, dst.end(), v_pointer);
			break;
		case TypeID::String:
			format.format_string(it, dst.end(), v_string);
			break;
		default:
			__assertion_fail_handler("unhandled typeID");
		}

		dst.remove_prefix(it - dst.begin());
	}
private:
	union
	{
		bool v_bool;
		char v_char;
		int32_t v_int32;
		uint32_t v_uint32;
		int64_t v_int64;
		uint64_t v_uint64;
		uintptr_t v_pointer;
		string_view v_string;
	};

	enum class TypeID
	{
		Bool = 0,
		Char,
		Int32,
		UInt32,
		Int64,
		UInt64,
		Pointer,
		String
	} type_id;
};

inline constexpr fmt_argument make_argument(const bool arg)
{
	return arg;
}

inline constexpr fmt_argument make_argument(const char arg)
{
	return arg;
}

inline constexpr fmt_argument make_argument(const int8_t arg)
{
	return static_cast<int32_t>(arg);
}

inline constexpr fmt_argument make_argument(const uint8_t arg)
{
	return static_cast<uint32_t>(arg);
}

inline constexpr fmt_argument make_argument(const int16_t arg)
{
	return static_cast<int32_t>(arg);
}

inline constexpr fmt_argument make_argument(const uint16_t arg)
{
	return static_cast<uint32_t>(arg);
}

inline constexpr fmt_argument make_argument(const int arg)
{
	return static_cast<int32_t>(arg);
}

inline constexpr fmt_argument make_argument(const unsigned int arg)
{
	return static_cast<uint32_t>(arg);
}

inline constexpr fmt_argument make_argument(const int64_t arg)
{
	return arg;
}

inline constexpr fmt_argument make_argument(const uint64_t arg)
{
	return arg;
}

inline constexpr fmt_argument make_argument(void* arg)
{
	return arg;
}

inline constexpr fmt_argument make_argument(const void* arg)
{
	return arg;
}

inline constexpr fmt_argument make_argument(const char* arg)
{
	return string_view(arg);
}

constexpr void parse_fmt_string(string_span& out_buffer, string_view& fmt)
{
	auto* out_it = out_buffer.begin();
	const auto* fmt_it = fmt.cbegin();

	while(fmt_it < fmt.cend() && out_it < out_buffer.end())
	{
		if(*fmt_it == '{')
		{
			if(*(fmt_it + 1) == '{')
			{
				++fmt_it;
				*out_it++ = *fmt_it++;
			}
			else
				break;
		}
		else if(*fmt_it == '}')
		{
			if(*(fmt_it + 1) != '}')
			{
				__assertion_fail_handler("*(fmt_it + 1) == '}'");
				return;
			}

			++fmt_it;
			*out_it++ = *fmt_it++;
		}
		else
		{
			*out_it++ = *fmt_it++;
		}
	}

	out_buffer.remove_prefix(out_it - out_buffer.begin());
	fmt.remove_prefix(fmt_it - fmt.cbegin());
}

constexpr void fmt_process(string_span& out_buffer, string_view& fmt, const fmt_argument* const args, const int arg_count)
{
	int arg_seq_index = 0;

	parse_fmt_string(out_buffer, fmt);

	while(!fmt.empty())
	{
		fmt_arg_format format(fmt, arg_count);
		int arg_index = format.index();

		if(arg_index < 0)
		{
			if(arg_seq_index >= arg_count)
			{
				__assertion_fail_handler("arg_seq_index < arg_count");
			       	return;
			}
			arg_index = arg_seq_index++;
		}
		args[arg_index].format(out_buffer, format);

		parse_fmt_string(out_buffer, fmt);	
	}
}

export template <typename... Args>
constexpr void format_to(string_span out_buffer, string_view fmt)
{
	parse_fmt_string(out_buffer, fmt);
	out_buffer[0] = '\0';
}

export template <typename... Args>
constexpr void format_to(string_span out_buffer, string_view fmt, Args&&... args)
{
	const fmt_argument arguments[sizeof...(Args)]{make_argument(args)...};
	fmt_process(out_buffer, fmt, arguments, static_cast<int>(sizeof...(Args))); 
	out_buffer[0] = '\0';
}
