#pragma once

#include <penumbra/gpu.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

inline void render_gpu_pass_begin(GPUCommandBuffer& cmd, render_gpu_pass pass)
{
	gpu_write_timestamp(cmd, (renderer_gfx_frame_index() * RENDER_GPU_PASS_BANK_SIZE) + (u32(pass) * 2));
}

inline void render_gpu_pass_end(GPUCommandBuffer& cmd, render_gpu_pass pass)
{
	gpu_write_timestamp(cmd, (renderer_gfx_frame_index() * RENDER_GPU_PASS_BANK_SIZE) + (u32(pass) * 2) + 1u);
}

}
