#pragma once

#include <penumbra/renderer.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/math/matrix.hpp>
#include <penumbra/types.hpp>
#include <vector>

namespace penumbra
{

struct render_csm_cascade
{
	GPUTexture texture;
	GPUTextureDescriptor descriptor;
	renderViewID render_view;
	u32 dim;
	float split_point;

	mat4 proj;
	mat4 view;
};

struct render_shadow_data
{
	u32 max_cascades{3u};
	float csm_lambda{0.9f};
	float csm_cbias{0.00125f};
	float csm_nbias{0.275f};
	float csm_scale{1.0f};
	
	std::vector<render_csm_cascade> cascades;
	GPUPointer smap_transforms;
};

void renderer_shadow_init(render_shadow_data& data);
void renderer_shadow_cleanup(render_shadow_data& data);

void renderer_shadow_update(render_shadow_data& data, const render_camera_data& main_camera, vec3 light_dir);
void renderer_shadow_build(render_shadow_data& data, GPUCommandBuffer& cmd);

}
