export module penumbra.gpu:utility;

import :api;
import std;

namespace penumbra
{

export constexpr uint32_t format_blockdim(GPUFormat fmt)
{
	switch(fmt)
	{
	case GPU_FORMAT_BC5_UNORM:
	case GPU_FORMAT_BC6H_UFLOAT:
	case GPU_FORMAT_BC7_UNORM:
	case GPU_FORMAT_BC7_SRGB:
		return 4u;
	default:
		return 1u;
	}
}

export constexpr uint32_t format_blocksize(GPUFormat fmt)
{
	switch(fmt)
	{
	case GPU_FORMAT_BC5_UNORM:
	case GPU_FORMAT_BC6H_UFLOAT:
	case GPU_FORMAT_BC7_UNORM:
	case GPU_FORMAT_BC7_SRGB:
		return 16u;
	case GPU_FORMAT_RGBA16_SFLOAT:
		return 8u;
	case GPU_FORMAT_R32_UINT:
	case GPU_FORMAT_RG16_SFLOAT:
	case GPU_FORMAT_RGBA8_SRGB:
	case GPU_FORMAT_RGBA8_UNORM:
	case GPU_FORMAT_BGRA8_SRGB:
	case GPU_FORMAT_B10GR11_UFLOAT:
		return 4u;
	case GPU_FORMAT_RG8_UNORM:
		return 2u;
	default:
		return 1u;
	}
}

export constexpr uint32_t size_for_image(uint32_t w, uint32_t h, uint32_t d, GPUFormat fmt)
{
	auto bdim = format_blockdim(fmt);
	auto wb = std::max(w / bdim, 1u);
	auto hb = std::max(h / bdim, 1u);
	auto db = std::max(d / bdim, 1u);
	return wb * hb * db * format_blocksize(fmt);
}

}
