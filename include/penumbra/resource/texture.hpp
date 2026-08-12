#pragma once

#include <penumbra/gpu.hpp>
#include <penumbra/types.hpp>
#include <string>

namespace penumbra
{

struct TextureFileFormat
{
	constexpr static u32 fmt_magic = 0x5845544c; //FIXME
	constexpr static u32 fmt_major = 2u;
	constexpr static u32 fmt_minor = 1u;

	enum class TextureFormat : u32
	{
		Invalid,
		BC4Unorm,
		BC5Unorm,
		BC6HUfloat,
		BC7Unorm,
		BC7SRGB
	};

	constexpr static GPUFormat parse_format(TextureFormat fmt)
	{
		switch(fmt)
		{
		case TextureFormat::Invalid:
			return GPU_FORMAT_UNDEFINED;
		case TextureFormat::BC4Unorm:
			return GPU_FORMAT_BC4_UNORM;
		case TextureFormat::BC5Unorm:
			return GPU_FORMAT_BC5_UNORM;
		case TextureFormat::BC6HUfloat:
			return GPU_FORMAT_BC6H_UFLOAT;
		case TextureFormat::BC7Unorm:
			return GPU_FORMAT_BC7_UNORM;
		case TextureFormat::BC7SRGB:
			return GPU_FORMAT_BC7_SRGB;
		}
	}

	struct Header
	{
		u32 magic{fmt_magic};
		u32 vmajor{fmt_major};
		u32 vminor{fmt_minor};
		TextureFormat texformat;
		u32 num_subres;
		u32 subres_desc_offset;
	};

	struct SubresourceDescription
	{
		u32 width;
		u32 height;
		u32 level;
		u32 layer;
		u32 data_offset;
		u32 data_size_bytes;
	};
};

struct texture_resource 
{
	std::string name;

	GPUTexture texture;
	GPUTextureDescriptor descriptor;
	u64 syncval;
};

}
