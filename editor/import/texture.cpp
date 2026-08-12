#include <import/resource.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/math/utility.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/types.hpp>

#include <ispc_texcomp.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace penumbra
{

constexpr u32 get_mip_levels(u32 w, u32 h)
{
	u32 result = 1;
	
	while(w > 4 || h > 4)
	{
		result++;
		w /= 2;
		h /= 2;
	}

	return result;
}

constexpr const GPUFormat src_format_from_type[] =
{
	GPU_FORMAT_RGBA8_SRGB,
	GPU_FORMAT_RGBA8_UNORM,
	GPU_FORMAT_RG8_UNORM,
	GPU_FORMAT_RGBA8_SRGB,
	GPU_FORMAT_RGBA16_SFLOAT
};

constexpr const GPUFormat dst_format_from_type[] =
{
	GPU_FORMAT_BC7_SRGB,
	GPU_FORMAT_BC7_UNORM,
	GPU_FORMAT_BC5_UNORM,
	GPU_FORMAT_BC7_SRGB,
	GPU_FORMAT_BC6H_UFLOAT
};

enum class ImageChannel
{
	none,
	R,
	G,
	B,
	A
};

constexpr const std::array<ImageChannel, 5> type_remaps[] =
{
	{ImageChannel::R, ImageChannel::G, ImageChannel::B, ImageChannel::A},
	{ImageChannel::G, ImageChannel::B, ImageChannel::R, ImageChannel::A},
	{ImageChannel::R, ImageChannel::G, ImageChannel::none, ImageChannel::none},
	{ImageChannel::R, ImageChannel::G, ImageChannel::B, ImageChannel::A},
	{ImageChannel::R, ImageChannel::G, ImageChannel::B, ImageChannel::A}
};

struct subresource_info
{
	u32 width;
	u32 height;
	u32 level;
	u32 layer;
	u32 byte_offset;
	u32 size_bytes;
};

struct texture_info
{
	std::span<u8> pixels;
	GPUFormat src_fmt;
	GPUFormat dst_fmt;
	std::span<subresource_info> src_subres;
	std::span<subresource_info> dst_subres;
};

template <typename T>
constexpr T* access_uv(const texture_info& tex, const subresource_info& subres, const uvec2& coords)
{
	u32 bd = gpu_format_blockdim(tex.src_fmt);
       	return reinterpret_cast<T*>(tex.pixels.data() + subres.byte_offset) + (coords.y * (subres.width + bd - 1) / bd) + coords.x;
}

struct InputFormatR8Unorm
{
	constexpr float sample(const texture_info& tex, const subresource_info& subres, const uvec2& coords) const noexcept
	{
		return static_cast<float>(*access_uv<u8>(tex, subres, coords)) * (1.0f / 255.0f);	}
	
	constexpr void write(const texture_info& tex, const subresource_info& subres, const uvec2& coords, float input) const noexcept
	{
		float val = std::clamp(std::round(input * 255.0f), 0.0f, 255.0f);
		*access_uv<u8>(tex, subres, coords) = static_cast<u8>(val);
	}
};

struct InputFormatRG8Unorm
{
	constexpr vec2 sample(const texture_info& tex, const subresource_info& subres, const uvec2& coords) const noexcept
	{
		bvec2& val = *access_uv<bvec2>(tex, subres, coords);
		return vec2{static_cast<float>(val.x), static_cast<float>(val.y)} * (1.0f / 255.0f);
	}

	constexpr void write(const texture_info& tex, const subresource_info& subres, const uvec2& coords, const vec2& input) const noexcept
	{
		vec2 val = vec2::clamp(vec2::round(input * 255.0f), vec2{0.0f}, vec2{255.0f});
		*access_uv<bvec2>(tex, subres, coords) = bvec2{static_cast<u8>(val.x), static_cast<u8>(val.y)};
	}
};

struct InputFormatRGBA8Unorm
{
	constexpr vec4 sample(const texture_info& tex, const subresource_info& subres, const uvec2& coords) const noexcept
	{
		bvec4& val = *access_uv<bvec4>(tex, subres, coords);
		return (1.0f / 255.0f) * vec4
		{
			static_cast<float>(val.x),
			static_cast<float>(val.y),
			static_cast<float>(val.z),
			static_cast<float>(val.w)
		};
	}

	constexpr void write(const texture_info& tex, const subresource_info& subres, const uvec2& coords, const vec4& input) const noexcept
	{
		vec4 val = vec4::clamp(vec4::round(input * 255.0f), vec4{0.0f}, vec4{255.0f});
		*access_uv<bvec4>(tex, subres, coords) = bvec4
		{
			static_cast<u8>(val.x),
			static_cast<u8>(val.y),
			static_cast<u8>(val.z),
			static_cast<u8>(val.w)
		};
	}
};

struct InputFormatRGBA8Srgb : public InputFormatRGBA8Unorm
{
	constexpr static float srgb_gamma_to_linear(float v)
	{
		if(v <= 0.04045f)
			return v * (1.0f / 12.92f);
		else
			return std::pow((v + 0.055f) / (1.0f + 0.055f), 2.4f);
	}

	constexpr static float srgb_linear_to_gamma(float v)
	{
		if(v <= 0.0031308f)
			return 12.92f * v;
		else
			return (1.0f + 0.055f) * std::pow(v, 1.0f / 2.4f) - 0.055f;
	}

	constexpr static vec4 srgb_gamma_to_linear(const vec4& v)
	{
		return vec4
		{
			srgb_gamma_to_linear(v.x),
			srgb_gamma_to_linear(v.y),
			srgb_gamma_to_linear(v.z),
			v.w
		};
	}

	constexpr static vec4 srgb_linear_to_gamma(const vec4& v)
	{
		return vec4
		{
			srgb_linear_to_gamma(v.x),
			srgb_linear_to_gamma(v.y),
			srgb_linear_to_gamma(v.z),
			v.w
		};
	}

	constexpr vec4 sample(const texture_info& tex, const subresource_info& subres, const uvec2& coords) const noexcept
	{
		return srgb_gamma_to_linear(InputFormatRGBA8Unorm::sample(tex, subres, coords));
	}

	constexpr void write(const texture_info& tex, const subresource_info& subres, const uvec2& coords, const vec4& input) const noexcept
	{
		InputFormatRGBA8Unorm::write(tex, subres, coords, srgb_linear_to_gamma(input));
	}
};

template <typename Fmt>
static void mipgen(const texture_info& tex, const Fmt& fmt)
{
	for(u32 level = 1; level < tex.src_subres.size(); level++)
	{
		auto& src = tex.src_subres[level - 1];
		auto& dst = tex.src_subres[level];

		u32 src_bd = gpu_format_blockdim(tex.src_fmt);
		u32 src_width = (src.width + src_bd - 1) / src_bd;
		u32 src_height = (src.height + src_bd - 1) / src_bd;

		u32 dst_width = (dst.width + src_bd - 1) / src_bd;
		u32 dst_height = (dst.height + src_bd - 1) / src_bd;

		uvec2 maxc{src_width - 1u, src_height - 1u};

		float rescale_w = static_cast<float>(src_width) / static_cast<float>(dst_width);
		float rescale_h = static_cast<float>(src_height) / static_cast<float>(dst_height);

		for(u32 y = 0; y < dst_height; y++)
		{
			float coordY = (static_cast<float>(y) + 0.5f) * rescale_h - 0.5f;
			for(u32 x = 0; x < dst_width; x++)
			{
				float coordX = (static_cast<float>(x) + 0.5f) * rescale_w - 0.5f;
				vec2 base = vec2{coordX, coordY};
				vec2 floor = {std::floor(base.x), std::floor(base.y)};
				vec2 uv = base - floor;

				uvec2 c0{static_cast<u32>(floor.x), static_cast<u32>(floor.y)};
				uvec2 c1 = uvec2::min(c0 + uvec2{1u, 0u}, maxc);
				uvec2 c2 = uvec2::min(c0 + uvec2{0u, 1u}, maxc);
				uvec2 c3 = uvec2::min(c0 + uvec2{1u, 1u}, maxc);

				auto v0 = fmt.sample(tex, src, c0);
				auto v1 = fmt.sample(tex, src, c1);
				auto v2 = fmt.sample(tex, src, c2);
				auto v3 = fmt.sample(tex, src, c3);

				auto x0 = mix(v0, v1, uv.x);
				auto x1 = mix(v2, v3, uv.x);
				auto filtered = mix(x0, x1, uv.y);
				fmt.write(tex, dst, uvec2(x, y), filtered);
			}
		}
	}
}

ResourceID import_texture(std::string_view name, import_texture_type type, std::span<const u8> data, uvec3 dim)
{
	u32 num_layers = (type == IMPORT_TEXTURE_CUBEMAP) ? 6u : 1u;
	u32 num_mips = get_mip_levels(dim.x, dim.y);

	u32 num_subres = num_mips * num_layers;

	std::vector<subresource_info> src_subres(num_subres);
	std::vector<subresource_info> dst_subres(num_subres);

	auto src_fmt = src_format_from_type[std::to_underlying(type)];
	auto dst_fmt = dst_format_from_type[std::to_underlying(type)];

	u32 s_acc = 0;
	u32 d_acc = 0;

	for(u32 i = 0; i < num_layers; i++)
	{
		src_subres[i] = {dim.w, dim.h, 0u, i, s_acc, gpu_format_size(src_fmt, dim.w, dim.h, 1u)};
		dst_subres[i] = {dim.w, dim.h, 0u, i, d_acc, gpu_format_size(dst_fmt, dim.w, dim.h, 1u)};

		s_acc += src_subres[i].size_bytes;
		d_acc += dst_subres[i].size_bytes;
	}

	for(u32 i = num_layers; i < num_subres; i += num_layers)
	{
		auto& sPrev = src_subres[i - num_layers];

		u32 nw = sPrev.width / 2;
		u32 nh = sPrev.height / 2;

		auto ss = gpu_format_size(src_fmt, nw, nh, 1u);
		auto ds = gpu_format_size(dst_fmt, nw, nh, 1u);

		for(u32 j = 0; j < num_layers; j++)
		{
			src_subres[i + j] = {nw, nh, (i / num_layers), j, s_acc, ss};
			dst_subres[i + j] = {nw, nh, (i / num_layers), j, d_acc, ds};

			s_acc += ss;
			d_acc += ds;
		}
	}

	u32 source_size = s_acc;
	u32 dest_size = d_acc;

	auto channel_map = type_remaps[std::to_underlying(type)];
	u32 required_channels = 0;
	for(auto c : channel_map)
	{
		if(c != ImageChannel::none)
			required_channels++;
	}

	std::vector<u8> pixels(source_size);
	u32 fmt_mul = (src_fmt == GPU_FORMAT_RGBA16_SFLOAT) ? 2u : 1u;
	u32 w_subres = num_layers > 1 ? num_subres : 1u;
	u32 data_offset = 0;

	for(u32 i = 0; i < w_subres; i++)
	{
		for(u32 y = 0, offset = 0; y < src_subres[i].height; y++)
		{
			for(u32 x = 0; x < src_subres[i].width; ++x, offset += (required_channels * fmt_mul))
			{
				for(u32 c = 0; c < dim.z; c++)
				{
					if(channel_map[c] == ImageChannel::none)
						continue;

					if(src_fmt == GPU_FORMAT_RGBA16_SFLOAT)
					{
						auto* fpx = data.data() + data_offset;
						u8 f0 = fpx[(x * dim.z * 2) + (y * src_subres[i].width * dim.z * 2) + (std::to_underlying(channel_map[c]) - 1u) * 2];
						u8 f1 = fpx[(x * dim.z * 2) + (y * src_subres[i].width * dim.z * 2) + (std::to_underlying(channel_map[c]) - 1u) * 2 + 1u];
						pixels[offset + (2 * c) + src_subres[i].byte_offset] = f0;
						pixels[offset + (2 * c + 1) + src_subres[i].byte_offset] = f1;
					}
					else
					{
						u8 bval = data[(x * dim.z) + (y * src_subres[i].width * dim.z) + std::to_underlying(channel_map[c]) - 1u];
						pixels[offset + c + src_subres[i].byte_offset] = bval;
					}
				}
			}
		}

		if(src_fmt == GPU_FORMAT_RGBA16_SFLOAT)
			data_offset += gpu_format_size(GPU_FORMAT_RGBA16_SFLOAT, src_subres[i].width, src_subres[i].height, 1u);
		else
			data_offset += gpu_format_size(GPU_FORMAT_RGBA8_UNORM, src_subres[i].width, src_subres[i].height, 1u);
	}

	texture_info mg_tex{pixels, src_fmt, dst_fmt, src_subres, dst_subres};

	if(num_layers == 1)
	{
	
	switch(src_fmt)
	{
	case GPU_FORMAT_R8_UNORM:
		mipgen(mg_tex, InputFormatR8Unorm());
		break;
	case GPU_FORMAT_RG8_UNORM:
		mipgen(mg_tex, InputFormatRG8Unorm());
		break;
	case GPU_FORMAT_RGBA8_UNORM:
		mipgen(mg_tex, InputFormatRGBA8Unorm());
		break;
	case GPU_FORMAT_RGBA8_SRGB:
		mipgen(mg_tex, InputFormatRGBA8Srgb());
		break;
	default:
		std::unreachable();
	}

	}

	std::vector<u8> encode_buffer(dest_size);

	bc7_enc_settings bc7 = {};
	switch(type)
	{
	case IMPORT_TEXTURE_ALBEDO:
		if(dim.z == 4)
			GetProfile_alpha_fast(&bc7);
		else
			GetProfile_fast(&bc7);
		break;
	default:
		GetProfile_fast(&bc7);
	}

	bc6h_enc_settings bc6h = {};
	GetProfile_bc6h_fast(&bc6h);

	for(u32 l = 0; l < num_subres; l++)
	{
		auto& src = src_subres[l];
		auto& dst = dst_subres[l];

		rgba_surface padded_surface = {};
		padded_surface.width = src.width;
		padded_surface.height = src.height;
		padded_surface.stride = src.width * gpu_format_blocksize(src_fmt);
		padded_surface.ptr = reinterpret_cast<u8*>(pixels.data()) + src.byte_offset;

		u8* encode_ptr = encode_buffer.data() + dst.byte_offset;
		
		switch(dst_fmt)
		{
		case GPU_FORMAT_BC7_SRGB:
		case GPU_FORMAT_BC7_UNORM:
			CompressBlocksBC7(&padded_surface, encode_ptr, &bc7);
			break;
		case GPU_FORMAT_BC6H_UFLOAT:
			CompressBlocksBC6H(&padded_surface, encode_ptr, &bc6h);
			break;
		case GPU_FORMAT_BC5_UNORM:
			CompressBlocksBC5(&padded_surface, encode_ptr);
			break;
		default:
			std::unreachable();
		}
	}

	return resource_manager_import_texture(name, 
	{
		.type = (num_layers) == 6 ? GPU_TEXTURE_CUBE : GPU_TEXTURE_2D,
		.dim = {dim.w, dim.h, 1u},
		.mip_count = num_mips,
		.layer_count = num_layers,
		.format = dst_fmt,
		.usage = GPU_TEXTURE_SAMPLED
	}, {reinterpret_cast<const u8*>(encode_buffer.data()), dest_size});
}

}
