export module penumbra.renderer:api;
import :geometry_buffer;
import :material;
import penumbra.core;
import penumbra.math;
import penumbra.gpu;

import std;
using std::uint64_t, std::size_t;

namespace penumbra
{

export using RenderView = strongly_typed<uint32_t, struct _render_view_tag>;
export using RenderObject = strongly_typed<uint32_t, struct _render_object_tag>;

export enum RenderBucket
{
	RENDER_BUCKET_DEFAULT,
	RENDER_BUCKET_DOUBLE_SIDED,
	RENDER_BUCKET_ALPHA_MASKED,
	RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED,
	RENDER_BUCKET_TRANSPARENT,
	RENDER_BUCKET_TRANSPARENT_DOUBLE_SIDED,
	RENDER_BUCKET_COUNT
};

export struct RenderObjectDescription
{
	Transform transform;
	RenderBucket bucket;
	uint32_t material_offset;
	uint32_t geom_l0_cluster_count;	
	uint32_t geom_lod_offset;
	uint32_t geom_lod_count;
};

export struct RenderBucketData
{
	GPUPointer commands;
	GPUPointer counter;
	GPUPointer instances;
	uint32_t max_instance_count;
};

export struct VisbufferInfo
{
	GPUTextureDescriptor* texture;
	GPUDevicePointer instances;
	GPUDevicePointer objects;
	uvec2 resolution;
};

export using visbuffer_read_hook = std::function<void(GPUCommandBuffer&, VisbufferInfo, uint32_t)>;

export void renderer_init(Window& wnd);
export void renderer_shutdown();

export void renderer_next_frame();
export void renderer_process_frame(double dt);
export uint32_t renderer_gfx_frame_index();
export uint64_t renderer_resource_transfer_syncval();
export void renderer_write_texture(GPUTexture texture, std::span<const std::byte> data, uint32_t num_mips = 1, uint32_t num_layers = 1);
export void renderer_write_material(const RenderMaterialData& data);
export GPUDevicePointer renderer_materials_device_pointer();
export RenderObject renderer_world_insert_object(const RenderObjectDescription& data);
export RenderBucketData renderer_world_get_bucket(RenderView view, RenderBucket bucket);
export GPUTextureDescriptor* renderer_get_framebuffer();
export uvec2 renderer_get_render_resolution();
export void renderer_update_render_resolution(uvec2 res);
export void renderer_update_camera(const mat4& view, const mat4& proj, float exposure);
export void renderer_add_visbuffer_hook(visbuffer_read_hook&& hook); 

}
