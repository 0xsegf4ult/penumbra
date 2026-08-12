#pragma once

#include <penumbra/types.hpp>
#include <string>

namespace penumbra
{

namespace fnv
{

constexpr u32 prime = 0x1000193u;
constexpr u32 basis = 0x811c9dc5u;

template <typename CharT>
constexpr size_t strlen_nonull(const CharT* str)
{
	size_t out = 0;
	while(str[++out] != '\0');
	return out;
}

template <typename CharT>
constexpr u32 hash(const CharT* str)
{
	u32 out = basis;
	size_t len = strlen_nonull(str);

	for(size_t i = 0; i < len; i++)
		out = (out ^ static_cast<u32>(str[i])) * prime;

	return out;
}

constexpr u32 hash(std::string_view str)
{
	u32 out = basis;

	for(auto c : str)
		out = (out ^ static_cast<u32>(c)) * prime;
	
	return out;
}

}

}

constexpr u32 operator""_fnv(const char* str)
{
	return penumbra::fnv::hash(str);
}
