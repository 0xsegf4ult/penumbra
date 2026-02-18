export module penumbra.renderer:api;
export import :geometry_buffer;
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

export void renderer_init(Window& wnd);
export void renderer_shutdown();

export void renderer_next_frame();
export void renderer_process_frame(double dt);
export uint32_t renderer_gfx_frame_index();
export uint64_t renderer_resource_transfer_syncval();
export RenderObject renderer_world_insert_object(const RenderObjectDescription& data);
export RenderBucketData renderer_world_get_bucket(RenderView view, RenderBucket bucket);
export void renderer_update_camera_matrices(const mat4& view, const mat4& proj);

}
