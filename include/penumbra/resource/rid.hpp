#pragma once

#include <penumbra/types.hpp>
#include <utility>

enum resource_type : u8
{
	RESOURCE_TYPE_INVALID,
	RESOURCE_TYPE_GEOMETRY,
	RESOURCE_TYPE_TEXTURE,
	RESOURCE_TYPE_MATERIAL,
	RESOURCE_TYPE_ANIMATION,
	RESOURCE_TYPE_SKELETON
};

using ResourceID = u32;
constexpr ResourceID resource_id_new(resource_type type, u32 handle)
{
	return std::to_underlying(type) << 24 | handle & 0xFFFFFF;
}

constexpr resource_type resource_get_type(ResourceID res)
{
	return resource_type{static_cast<u8>(res >> 24)};
}

constexpr u32 resource_get_handle(ResourceID res)
{
	return res & 0xFFFFFF;
}

