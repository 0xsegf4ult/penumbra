module;

#include <cassert>

export module penumbra.renderer;

import penumbra.core;
import penumbra.gpu;
import penumbra.ui;

import std;
using std::uint64_t, std::size_t;

namespace penumbra
{

constexpr size_t frames_in_flight = 2;

struct renderer_context_t
{
	Window* window;

	std::array<uint64_t, frames_in_flight> gfx_queue_frames;
	std::array<uint64_t, frames_in_flight> compute_queue_frames;
	int frame_index;

	std::array<GPUSemaphore, frames_in_flight> swapchain_acquire;
	std::array<GPUSemaphore, frames_in_flight> swapchain_present;
	GPUTextureDescriptor* cur_swapchain;
};

renderer_context_t* renderer = nullptr;

export void renderer_init(Window& wnd)
{
	renderer = new renderer_context_t();
	renderer->window = &wnd;

	gpu_init();
	gpu_swapchain_init(wnd);
	renderer->frame_index = 0;
	
	for(int i = 0; i < frames_in_flight; i++)
	{
		renderer->gfx_queue_frames[i] = 0;
		renderer->compute_queue_frames[i] = 0;

		renderer->swapchain_acquire[i] = gpu_create_semaphore(0, GPU_SEMAPHORE_BINARY);
		renderer->swapchain_present[i] = gpu_create_semaphore(0, GPU_SEMAPHORE_BINARY);
	}

	imgui_backend_init(renderer->window);
}

export void renderer_shutdown()
{
	gpu_wait_idle();

	imgui_backend_shutdown();

	for(int i = 0; i < frames_in_flight; i++)
	{
		gpu_destroy_semaphore(renderer->swapchain_acquire[i]);
		gpu_destroy_semaphore(renderer->swapchain_present[i]);
	}

	gpu_shutdown();
	delete renderer;
}

export void renderer_next_frame()
{
	renderer->frame_index = (renderer->frame_index + 1) % 2;
	gpu_wait_queue(GPU_QUEUE_GRAPHICS, renderer->gfx_queue_frames[renderer->frame_index]);
	gpu_wait_queue(GPU_QUEUE_COMPUTE, renderer->compute_queue_frames[renderer->frame_index]);
	renderer->cur_swapchain = gpu_swapchain_acquire_next(renderer->swapchain_acquire[renderer->frame_index]);
}

export void renderer_process_frame(double dt)
{
	assert(renderer->cur_swapchain);
	assert(renderer->cur_swapchain->texture);
	auto wdim = renderer->cur_swapchain->texture->size;

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_wait_signal(cmd, GPU_STAGE_RASTER_OUTPUT, renderer->swapchain_acquire[renderer->frame_index], 0);
	gpu_texture_layout_transition(cmd, *renderer->cur_swapchain, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_RASTER_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	gpu_begin_renderpass(cmd,
	{
		.render_area = {0, 0, wdim.w, wdim.h},
		.color_targets =
		{
			{renderer->cur_swapchain, GPU_LOAD_OP_CLEAR}
		}
	});

	imgui_backend_render(cmd, dt);

	gpu_end_renderpass(cmd);

	gpu_texture_layout_transition(cmd, *renderer->cur_swapchain, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_ALL, GPU_TEXTURE_LAYOUT_GENERAL, GPU_TEXTURE_LAYOUT_PRESENT);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->swapchain_present[renderer->frame_index], 0);
	auto gfx_sync = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	renderer->gfx_queue_frames[renderer->frame_index] = gfx_sync;

	gpu_swapchain_present(GPU_QUEUE_GRAPHICS, renderer->swapchain_present[renderer->frame_index]);
}

}
