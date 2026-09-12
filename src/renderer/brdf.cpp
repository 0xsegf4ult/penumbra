#include <renderer/brdf.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/log.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

static GPUPipeline brdflut_pso;
static GPUTexture brdflut_tex;

GPUTextureDescriptor renderer_brdf_init()
{
	brdflut_pso = gpu_create_graphics_pipeline(load_shader("shaders/brdflut"),
	{
		.color_targets = {GPU_FORMAT_RG16_SFLOAT}
	});

	log::info("renderer: generating BRDF lookup table: 512x512 1024 integration steps");
	brdflut_tex = gpu_create_texture
	({
		.dim = {512u, 512u, 1u},
		.format = GPU_FORMAT_RG16_SFLOAT,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_COLOR_ATTACHMENT
	});
	auto descriptor = gpu_texture_view_descriptor(brdflut_tex, {.format = GPU_FORMAT_RG16_SFLOAT});
	
	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, brdflut_tex, GPU_STAGE_NONE, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);

	gpu_begin_renderpass(cmd,
	{
		.color_targets = {{brdflut_tex}}
	});

	gpu_set_pipeline(cmd, brdflut_pso);
	gpu_draw(cmd, nullptr, 3, 1, 0, 0);
	gpu_end_renderpass(cmd);

	gpu_barrier(cmd, GPU_STAGE_RASTER_COLOR_OUTPUT, GPU_STAGE_COMPUTE);
	gpu_submit(GPU_QUEUE_GRAPHICS, cmd);

	return descriptor;
}

void renderer_brdf_cleanup()
{
	gpu_destroy_texture(brdflut_tex);
	gpu_destroy_pipeline(brdflut_pso);
}

}
