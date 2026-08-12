#pragma once

#include <penumbra/math/vector.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/types.hpp>

#include <vector>

namespace penumbra
{

struct render_bloom_data
{
	GPUTexture tmp_quarter_rt_tex;
	GPUTextureDescriptor tmp_quarter_rt;
	std::vector<GPUTextureDescriptor> tmp_quarter_rt_rw;

	GPUTexture bloombuffer_tex;
	GPUTextureDescriptor bloombuffer;
	std::vector<GPUTextureDescriptor> bloombuffer_rw;

	u32 bloom_mips;

	uvec2 resolution;
};

void renderer_bloom_init(render_bloom_data& data);
void renderer_bloom_cleanup(render_bloom_data& data);

void renderer_bloom_init_rendertarget(render_bloom_data& data, GPUCommandBuffer& cmd, uvec2 resolution);
void renderer_bloom_cleanup_rendertarget(render_bloom_data& data);

void renderer_bloom_process(render_bloom_data& data, GPUCommandBuffer& cmd, GPUTextureDescriptor& input); 

}
