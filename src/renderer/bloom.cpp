#include <renderer/bloom.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace penumbra
{

static GPUPipeline bloom_hdrfilter_cs;
static GPUPipeline bloom_filter_cs;

void renderer_bloom_init(render_bloom_data& data)
{
	bloom_hdrfilter_cs = gpu_create_compute_pipeline(load_shader("shaders/bloom_filterHDR"));
	bloom_filter_cs = gpu_create_compute_pipeline(load_shader("shaders/bloom_filter"));
}

void renderer_bloom_cleanup(render_bloom_data& data)
{
	gpu_destroy_pipeline(bloom_filter_cs);
	gpu_destroy_pipeline(bloom_hdrfilter_cs);
}

void renderer_bloom_init_rendertarget(render_bloom_data& data, GPUCommandBuffer& cmd, uvec2 resolution)
{
	data.resolution = resolution;
	data.bloom_mips = std::min(5u, u32(std::log2(std::max(data.resolution.x, data.resolution.y))));

	data.tmp_quarter_rt_tex = gpu_create_texture
	({
		.dim = uvec3{data.resolution, 1u},
		.mip_count = data.bloom_mips,
		.format = GPU_FORMAT_B10GR11_UFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_STORAGE
	});
	data.tmp_quarter_rt = gpu_texture_view_descriptor(data.tmp_quarter_rt_tex, {.format = GPU_FORMAT_B10GR11_UFLOAT});
	data.tmp_quarter_rt_rw.resize(data.bloom_mips);
	for(u8 i = 0; i < data.bloom_mips; i++)
	{
		data.tmp_quarter_rt_rw[i] = 
		gpu_rwtexture_view_descriptor(data.tmp_quarter_rt_tex, 
		{
			.format = GPU_FORMAT_B10GR11_UFLOAT, 
			.base_mip = i,
			.mip_count = 1u
		});
	}

	data.bloombuffer_tex = gpu_create_texture
	({
		.dim = uvec3{data.resolution, 1u},
		.mip_count = data.bloom_mips,
		.format = GPU_FORMAT_B10GR11_UFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_STORAGE
	});
	data.bloombuffer = gpu_texture_view_descriptor(data.bloombuffer_tex, {.format = GPU_FORMAT_B10GR11_UFLOAT});
	data.bloombuffer_rw.resize(data.bloom_mips);
	for(u8 i = 0; i < data.bloom_mips; i++)
	{
		data.bloombuffer_rw[i] =
		gpu_rwtexture_view_descriptor(data.bloombuffer_tex,
		{
			.format = GPU_FORMAT_B10GR11_UFLOAT,
			.base_mip = i,
			.mip_count = 1u
		});
	}

	gpu_texture_layout_transition(cmd, data.tmp_quarter_rt_tex, GPU_STAGE_NONE, GPU_STAGE_COMPUTE, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_texture_layout_transition(cmd, data.bloombuffer_tex, GPU_STAGE_NONE, GPU_STAGE_COMPUTE, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
}

void renderer_bloom_cleanup_rendertarget(render_bloom_data& data)
{
	gpu_destroy_texture(data.bloombuffer_tex);
	gpu_free_descriptor(data.bloombuffer);
	for(auto& desc : data.bloombuffer_rw)
		gpu_free_descriptor(desc);

	gpu_destroy_texture(data.tmp_quarter_rt_tex);
	gpu_free_descriptor(data.tmp_quarter_rt);
	for(auto& desc : data.tmp_quarter_rt_rw)
		gpu_free_descriptor(desc);
}

void renderer_bloom_process(render_bloom_data& data, GPUCommandBuffer& cmd, GPUTextureDescriptor& input)
{
	gpu_set_pipeline(cmd, bloom_hdrfilter_cs);

	struct BloomHDRData
	{
		u32 input;
		u32 output;
		vec2 inv_res;
		float threshold;
		float max_value;
	} bhdr_data;
	bhdr_data.input = input.handle;
       	bhdr_data.output = data.bloombuffer_rw[0].handle;
	bhdr_data.inv_res = vec2{1.0f / float(data.resolution.x), 1.0f / float(data.resolution.y)};
	bhdr_data.threshold = 1.0f;
	bhdr_data.max_value = 10.0f;

	gpu_dispatch(cmd, &bhdr_data, {(data.resolution.x + 7u) / 8u, (data.resolution.y + 7u) / 8u, 1u});

	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

	struct BloomFilterData
	{
		u32 input;
		u32 output;
		uvec2 res;
		vec2 inv_res;
		vec2 direction;
		int level;
	} bf_data;
	bf_data.res = data.resolution;
	bf_data.inv_res = bhdr_data.inv_res;
	bf_data.level = 0;

	gpu_set_pipeline(cmd, bloom_filter_cs);

	for(u32 i = 0; i < data.bloom_mips - 1; i++)
	{
		bf_data.input = data.bloombuffer.handle;
		bf_data.output = data.tmp_quarter_rt_rw[i + 1].handle;
		bf_data.res /= 2;
		bf_data.inv_res *= 2.0f;
		bf_data.direction = {1.0f, 0.0f};

		gpu_dispatch(cmd, &bf_data, {(bf_data.res.x + 255u) / 256u, bf_data.res.y, 1u});

		gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

		bf_data.direction = {0.0f, 1.0f};
		bf_data.level = i + 1;
		bf_data.input = data.tmp_quarter_rt.handle;
		bf_data.output = data.bloombuffer_rw[i + 1].handle;

		gpu_dispatch(cmd, &bf_data, {bf_data.res.x, (bf_data.res.y + 255u) / 256u, 1u});

		if(i < data.bloom_mips - 2)
			gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);
	}
}

}
