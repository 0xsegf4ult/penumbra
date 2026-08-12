#pragma once

#include <widgets/widget.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct WorldState;

enum class ViewportTool
{
	Translate,
	Rotate,
	Scale,
	Fly
};

class Viewport : public Widget
{
public:
	Viewport(GPUTextureDescriptor* rt, WorldState* ws);
	~Viewport() override;

	void update_render_target(GPUTextureDescriptor* rt);
	void update_camera(const mat4& v, const mat4& p);
	void configure() override;

	void set_tool(ViewportTool tool);
	ViewportTool get_tool() const
	{
		return tool;
	}

	void on_draw() override;
private:
	void transform_gizmo(float root_x, float root_y);
	void object_picking(GPUCommandBuffer& cmd, visbuffer_data vb_data, u32 frame_index);

	GPUTextureDescriptor* render_target{nullptr};
	WorldState* world{nullptr};

	mat4 view;
	mat4 proj;

	GPUPipeline vb_picking_cs;
	GPUPointer picking_buffer;
	uvec2 picking_pos{0u, 0u};

	ViewportTool tool{ViewportTool::Translate};
};

}
