export module penumbra.resource:texture;

import :resource_id;

import penumbra.math;
import penumbra.gpu;
import std;
using std::uint32_t, std::uint64_t;

namespace penumbra
{

export struct TextureFileFormat
{
	constexpr static uint32_t fmt_magic = 0x5845544c; //FIXME
	constexpr static uint32_t fmt_major_version = 2u;
	constexpr static uint32_t fmt_minor_version = 1u;

	enum class TextureFormat : uint32_t
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
		uint32_t magic{fmt_magic};
		uint32_t vmajor{fmt_major_version};
		uint32_t vminor{fmt_minor_version};
		TextureFormat texformat;
		uint32_t num_subres;
		uint32_t subres_desc_offset;
	};

	struct SubresourceDescription
	{
		uint32_t width;
		uint32_t height;
		uint32_t level;
		uint32_t layer;
		uint32_t data_offset;
		uint32_t data_size_bytes;
	};
};

export struct TextureResource
{
	std::string name;

	GPUTexture texture;
	GPUTextureDescriptor descriptor;
	uint64_t syncval;
};

}
