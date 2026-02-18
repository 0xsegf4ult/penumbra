module;

#include <cassert>
#include <tracy/Tracy.hpp>

module penumbra.renderer;
import :geometry_buffer;

import penumbra.core;
import penumbra.gpu;
import penumbra.ui;

import std;
using std::uint64_t, std::size_t;

namespace penumbra
{

struct renderer_context_t
{
	Window* window;

	std::array<uint64_t, config::renderer_frames_in_flight> gfx_queue_frames;
	std::array<uint64_t, config::renderer_frames_in_flight> compute_queue_frames;
	int frame_index;

	std::array<GPUSemaphore, config::renderer_frames_in_flight> swapchain_acquire;
	std::array<GPUSemaphore, config::renderer_frames_in_flight> swapchain_present;
	GPUTexture cur_swapchain;

	GPUSemaphore transfer_resource_semaphore;
	uint64_t transfer_resource_sync{0};
};

renderer_context_t* renderer = nullptr;

void renderer_init(Window& wnd)
{
	renderer = new renderer_context_t();
	renderer->window = &wnd;

	gpu_init();
	gpu_swapchain_init(wnd);
	renderer->frame_index = 0;
	
	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		renderer->gfx_queue_frames[i] = 0;
		renderer->compute_queue_frames[i] = 0;

		renderer->swapchain_acquire[i] = gpu_create_semaphore(0, GPU_SEMAPHORE_BINARY);
		renderer->swapchain_present[i] = gpu_create_semaphore(0, GPU_SEMAPHORE_BINARY);
	}

	renderer->transfer_resource_semaphore = gpu_create_semaphore(0);
	
	imgui_backend_init(renderer->window);

	renderer_geometry_init();
}

void renderer_shutdown()
{
	gpu_wait_idle();

	renderer_geometry_shutdown();

	imgui_backend_shutdown();

	gpu_destroy_semaphore(renderer->transfer_resource_semaphore);

	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		gpu_destroy_semaphore(renderer->swapchain_acquire[i]);
		gpu_destroy_semaphore(renderer->swapchain_present[i]);
	}

	delete renderer;
	
	gpu_shutdown();
}

void renderer_next_frame()
{
	ZoneScoped;

	renderer->frame_index = (renderer->frame_index + 1) % config::renderer_frames_in_flight;
	if(!gpu_wait_queue(GPU_QUEUE_GRAPHICS, renderer->gfx_queue_frames[renderer->frame_index]))
		std::abort();

	gpu_wait_queue(GPU_QUEUE_COMPUTE, renderer->compute_queue_frames[renderer->frame_index]);
	renderer->cur_swapchain = gpu_swapchain_acquire_next(renderer->swapchain_acquire[renderer->frame_index]);
}

void renderer_copy_resources_async()
{
	if(!renderer_geometry_needs_upload())
		return;

	auto cmd = gpu_record_commands(GPU_QUEUE_TRANSFER);
	renderer_geometry_copy_async(cmd);
	renderer->transfer_resource_sync++;
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->transfer_resource_semaphore, renderer->transfer_resource_sync);
	auto transfer_sync = gpu_submit(GPU_QUEUE_TRANSFER, cmd);
}

void renderer_process_frame(double dt)
{
	ZoneScoped;
	renderer_copy_resources_async();
	
	assert(renderer->cur_swapchain);

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_wait_signal(cmd, GPU_STAGE_RASTER_OUTPUT, renderer->swapchain_acquire[renderer->frame_index], 0);
	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_RASTER_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	gpu_begin_renderpass(cmd,
	{
		.color_targets =
		{
			{
			.texture = renderer->cur_swapchain, 
			.load_op = GPU_LOAD_OP_CLEAR
			}
		}
	});

	imgui_backend_render(cmd, dt);

	gpu_end_renderpass(cmd);

	gpu_texture_layout_transition(cmd, renderer->cur_swapchain, GPU_STAGE_RASTER_OUTPUT, GPU_STAGE_ALL, GPU_TEXTURE_LAYOUT_GENERAL, GPU_TEXTURE_LAYOUT_PRESENT);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, renderer->swapchain_present[renderer->frame_index], 0);
	auto gfx_sync = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	renderer->gfx_queue_frames[renderer->frame_index] = gfx_sync;

	gpu_swapchain_present(GPU_QUEUE_GRAPHICS, renderer->swapchain_present[renderer->frame_index]);
}

uint32_t renderer_gfx_frame_index()
{
	return renderer->frame_index;
}

uint64_t renderer_resource_transfer_syncval()
{
	return renderer->transfer_resource_sync;
}

}
