#include <renderer/world.hpp>
#include <renderer/resource.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/config.hpp>
#include <penumbra/log.hpp>
#include <penumbra/types.hpp>
#include <penumbra/math/plane.hpp>
#include <penumbra/math/transform.hpp>

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cassert>
#include <map>
#include <vector>

namespace penumbra
{

constexpr size_t CULL_KERNEL_SIZE = 512;

enum renderview_flags
{
	RENDER_VIEW_FRUSTUM_CULL = 0x1,
	RENDER_VIEW_CONE_CULL = 0x2,
	RENDER_VIEW_OCCLUSION_CULL = 0x4,
	RENDER_VIEW_ORTHOGRAPHIC = 0x8
};

struct renderview_cbuffer
{
	mat4 viewmat;
	vec4 cam_pos;
	vec4 frustum_planes[4];
	float lod_base;
	float lod_step;
	float znear;
	float zfar;
	u32 flags;
	u32 lod_bias;
};

struct render_view
{
	u32 instance_capacity = 16384u;
	u32 instance_count = 0u;
	u32 primitive_capacity = 65536u;

	GPUPointer instances;
	GPUPointer clusters;
	GPUPointer visibility;
	GPUPointer commands;

	GPUPointer buckets[config::renderer_frames_in_flight];
	GPUPointer cbuffer[config::renderer_frames_in_flight];

	std::vector<GPUPointer> visibility_sums;
	u32 intermediate_sizes[16];

	u32 cluster_bucket_sizes[RENDER_BUCKET_COUNT];
	u32 cluster_bucket_offsets[RENDER_BUCKET_COUNT];

	u32 flags;
	u32 lod_bias{0};
	bool is_shadow;
	bool freeze_culling{false};
};

struct render_object_data
{
	mat4 transform;
	vec4 sphere;
	float cull_scale;
	u32 material_offset;
	u32 geom_lod_offset;
	u32 pack_bucket_lod_count;
	u32 geom_vtx_offset;
	u32 geom_idx_offset;
	u32 geom_cluster_offset;
	u32 flags;
};

struct render_world
{
	u32 object_capacity = 16384u;
	u32 object_count = 0u;

	GPUPointer host_objects;
	GPUPointer objects;

	std::vector<renderObjectID> dirty_objects;
	std::vector<render_view> views;

	//FIXME: sparse set might be better?
	std::map<renderObjectID, renderer_skinned_geometry_instance> sg_instances;
};

static render_world* world = nullptr;;

static GPUPipeline skinning_cs;
static GPUPipeline instance_cull_cs;
static GPUPipeline cluster_cull_cs;
static GPUPipeline cmdgen_cs;
static GPUPipeline ps_index_cs;
static GPUPipeline ps_partial_cs;

void renderer_world_init()
{
	world = new render_world();

	skinning_cs = gpu_create_compute_pipeline(load_shader("shaders/geometry_skinning"));
	instance_cull_cs = gpu_create_compute_pipeline(load_shader("shaders/instance_cull"));
	cluster_cull_cs = gpu_create_compute_pipeline(load_shader("shaders/cluster_cull"));
	cmdgen_cs = gpu_create_compute_pipeline(load_shader("shaders/generate_commands"));
	ps_index_cs = gpu_create_compute_pipeline(load_shader("shaders/prefix_scan_index"));
	ps_partial_cs = gpu_create_compute_pipeline(load_shader("shaders/prefix_scan_add_partial"));

	world->host_objects = gpu_allocate_memory(sizeof(render_object_data) * world->object_capacity, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	world->objects = gpu_allocate_memory(sizeof(render_object_data) * world->object_capacity);
}

static void renderer_destroy_view(render_view& view)
{
	for(auto& mem : view.visibility_sums)
		gpu_free_memory(mem);

	for(auto& elem : view.cbuffer)
		gpu_free_memory(elem);

	gpu_free_memory(view.commands);
	gpu_free_memory(view.visibility);
	for(auto& b : view.buckets)
		gpu_free_memory(b);

	gpu_free_memory(view.clusters);
	gpu_free_memory(view.instances);
}

void renderer_world_cleanup()
{
	for(auto& view : world->views)
		renderer_destroy_view(view);

	gpu_free_memory(world->objects);
	gpu_free_memory(world->host_objects);
	
	delete world;
	
	gpu_destroy_pipeline(ps_partial_cs);
	gpu_destroy_pipeline(ps_index_cs);
	gpu_destroy_pipeline(cmdgen_cs);
	gpu_destroy_pipeline(cluster_cull_cs);
	gpu_destroy_pipeline(instance_cull_cs);
	gpu_destroy_pipeline(skinning_cs);
}

renderViewID renderer_create_view(const render_view_desc& desc)
{
	world->views.push_back(render_view{});
	auto& view = world->views.back();

	view.instances = gpu_allocate_memory(sizeof(uvec2) * view.instance_capacity, GPU_MEMORY_MAPPED);
	view.clusters = gpu_allocate_memory(sizeof(uvec2) * view.primitive_capacity);
	view.visibility = gpu_allocate_memory(sizeof(u32) * view.primitive_capacity);
	view.commands = gpu_allocate_memory(sizeof(GPUIndexedIndirectCommand) * view.primitive_capacity, GPU_MEMORY_PRIVATE, GPU_BUFFER_INDIRECT);

	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		view.buckets[i] = gpu_allocate_memory(sizeof(uvec2) * RENDER_BUCKET_COUNT, GPU_MEMORY_MAPPED, GPU_BUFFER_INDIRECT);
		view.cbuffer[i] = gpu_allocate_memory(sizeof(renderview_cbuffer), GPU_MEMORY_MAPPED, GPU_BUFFER_UNIFORM);
	}

	view.visibility_sums.push_back(gpu_allocate_memory(sizeof(u32) * view.primitive_capacity));
	u32 n = (view.primitive_capacity / CULL_KERNEL_SIZE) + 1u;
	while(n > 1)
	{
		view.visibility_sums.push_back(gpu_allocate_memory(sizeof(u32) * n));
		n = (n / CULL_KERNEL_SIZE) + 1u;
	}
	
	view.visibility_sums.push_back(gpu_allocate_memory(sizeof(u32)));
	view.is_shadow = desc.is_shadow;
	view.flags = RENDER_VIEW_FRUSTUM_CULL;
	if(!desc.is_shadow)
		view.flags |= RENDER_VIEW_CONE_CULL;

	return renderViewID{static_cast<u32>(world->views.size())};
}

void renderer_update_view(renderViewID id, const render_camera_data& cam)
{
	ZoneScoped;

	assert(id);

	auto& view = world->views[id - 1];

	if(view.freeze_culling)
		return;

	auto* cbuffer = reinterpret_cast<renderview_cbuffer*>(gpu_map_memory(view.cbuffer[renderer_gfx_frame_index()]));
	cbuffer->viewmat = cam.view;
	cbuffer->cam_pos = vec4{cam.position, 1.0f};

	mat4 projT = mat4::transpose(cam.proj);

	if(!view.is_shadow)
	{
		const vec4 frustumX = Plane(projT[3] + projT[0]).normalize().as_vector();
		const vec4 frustumY = Plane(projT[3] + projT[1]).normalize().as_vector();
		cbuffer->frustum_planes[0] = vec4{frustumX.x, frustumX.z, frustumY.y, frustumY.z};
	}
	else
	{
		cbuffer->frustum_planes[0] = Plane(projT[3] + projT[0]).normalize().as_vector();
		cbuffer->frustum_planes[1] = Plane(projT[3] - projT[0]).normalize().as_vector();
		cbuffer->frustum_planes[2] = Plane(projT[3] + projT[1]).normalize().as_vector();
		cbuffer->frustum_planes[3] = Plane(projT[3] - projT[1]).normalize().as_vector();
	}

	cbuffer->znear = cam.znear;
	cbuffer->zfar = cam.zfar;
}

render_bucket determine_bucket(u32 mtl_flags)
{
	if(mtl_flags & MATERIAL_ALPHA_MASK)
	{
		if(mtl_flags & MATERIAL_DOUBLE_SIDED)
			return RENDER_BUCKET_ALPHA_MASKED_DOUBLE_SIDED;

		return RENDER_BUCKET_ALPHA_MASKED;
	}

	if(mtl_flags & MATERIAL_ALPHA_BLEND)
	{
		if(mtl_flags & MATERIAL_DOUBLE_SIDED)
			return RENDER_BUCKET_TRANSPARENT_DOUBLE_SIDED;

		return RENDER_BUCKET_TRANSPARENT;
	}

	if(mtl_flags & MATERIAL_DOUBLE_SIDED)
		return RENDER_BUCKET_DOUBLE_SIDED;

	return RENDER_BUCKET_DEFAULT;
}

renderObjectID renderer_world_insert_object_internal(const render_object_desc& desc, array_proxy<renderViewID> views)
{
	ZoneScoped;

	if(world->object_count >= world->object_capacity)
	{
		log::warn("render_world: object storage capacity [{}] exceeded", world->object_capacity);
		return renderObjectID{0};
	}
	

	render_object_data* obj = reinterpret_cast<render_object_data*>(gpu_map_memory(world->host_objects)) + world->object_count;
	
	world->object_count++;
	renderObjectID handle{world->object_count};
	world->dirty_objects.push_back(handle);

	obj->transform = desc.transform;

	const vec3 scale = {desc.transform.row(0u).magnitude(), desc.transform.row(1u).magnitude(), desc.transform.row(2u).magnitude()};
	obj->cull_scale = std::max(std::max(std::abs(scale.x), std::abs(scale.y)), std::abs(scale.z));

	auto& geom_data = resource_manager_get_geometry(desc.geometry);
	bool is_skinned = resource_get_handle(desc.skeleton);
	
	u32 vtx_offset = geom_data.vertex_offset;
	if(is_skinned)
	{
		auto sg_instance = renderer_geometry_instantiate_skin(vtx_offset, geom_data.vertex_count, resource_manager_get_skeleton(desc.skeleton).bone_count);
		vtx_offset = sg_instance.vertex_offset;
		world->sg_instances[handle] = sg_instance;
	}

	obj->sphere = geom_data.sphere;
	obj->material_offset = resource_get_handle(desc.material);
	render_bucket bucket = RENDER_BUCKET_DEFAULT;
	if(obj->material_offset)
	{
		auto& mtl_data = resource_manager_get_material(desc.material);
		bucket = determine_bucket(mtl_data.flags);
	}

	obj->geom_lod_offset = geom_data.lod_offset; 
	obj->pack_bucket_lod_count = (bucket << 16) | geom_data.lod_count;
	obj->geom_vtx_offset = vtx_offset;
	obj->geom_idx_offset = geom_data.index_offset;
	obj->geom_cluster_offset = geom_data.cluster_offset;

	for(auto view_handle : views)
	{
		if(!view_handle)
			continue;

		render_view& view = world->views[view_handle - 1];
		auto bucket_offset = std::to_underlying(bucket);
		auto* instance = reinterpret_cast<uvec2*>(gpu_map_memory(view.instances)) + view.instance_count;
		*instance = {handle, view.cluster_bucket_sizes[bucket_offset]};
	       	view.instance_count++;
		view.cluster_bucket_sizes[bucket_offset] += geom_data.l0_cluster_count;
	}

	return handle;	
}

void renderer_world_update_object(renderObjectID object, const mat4& transform)
{
	ZoneScoped;

	assert(object);

	auto* data = reinterpret_cast<render_object_data*>(gpu_map_memory(world->host_objects)) + (object - 1);
	data->transform = transform;
	const vec3 scale = {transform.row(0u).magnitude(), transform.row(1u).magnitude(), transform.row(2u).magnitude()};
	data->cull_scale = std::max(std::max(std::abs(scale.x), std::abs(scale.y)), std::abs(scale.z));

	world->dirty_objects.push_back(object);
}

void renderer_world_update_skin(renderObjectID object, const mat4* bones, u16 count)
{
	ZoneScoped;

	assert(object);

	auto& data = world->sg_instances[object];
	renderer_write_bones(data.bone_offset, bones, count);
}

static void renderer_world_skinning(GPUCommandBuffer& cmd)
{
	if(world->sg_instances.empty())
		return;

	auto geometry_storage = renderer_geometry_get_storage();
	GPUDevicePointer skv = gpu_host_to_device_pointer(geometry_storage.vertex_skin);
	GPUDevicePointer vpos = gpu_host_to_device_pointer(geometry_storage.vertex_pos);
	GPUDevicePointer vuv = gpu_host_to_device_pointer(geometry_storage.vertex_uv);
	GPUDevicePointer vnorm = gpu_host_to_device_pointer(geometry_storage.vertex_nor_tan);
	GPUDevicePointer bone = gpu_host_to_device_pointer(renderer_bones_get_storage());

	struct SkinningData
	{
		GPUDevicePointer vertex_skinned;
		GPUDevicePointer vertex_pos;
		GPUDevicePointer vertex_uv;
		GPUDevicePointer vertex_nor_tan;
		GPUDevicePointer bone;
		u32 vertex_count;
	} shader_data;

	gpu_set_pipeline(cmd, skinning_cs);
	for(auto& [obj, sm] : world->sg_instances)
	{
		shader_data.vertex_skinned = skv + (sm.vertex_skinned_offset * sizeof(geom_skinned_format));
		shader_data.vertex_pos = vpos + (sm.vertex_offset * sizeof(geom_position_format));
		shader_data.vertex_uv = vuv + (sm.vertex_offset * sizeof(geom_uv_format));
		shader_data.vertex_nor_tan = vnorm + (sm.vertex_offset * sizeof(geom_nor_tan_format));
		shader_data.bone = bone + (sm.bone_offset * sizeof(mat4));
		shader_data.vertex_count = sm.vertex_count;

		gpu_dispatch(cmd, &shader_data, {(sm.vertex_count + 31u) / 32u, 1u, 1u});
	}
}

void renderer_world_update(GPUCommandBuffer& cmd)
{
	ZoneScoped;

	renderer_world_skinning(cmd);

	if(world->dirty_objects.empty())
		return;

	if(world->dirty_objects.size() == world->object_count)
	{
		gpu_mem_copy(cmd, world->host_objects, world->objects, world->object_count * sizeof(render_object_data));
		world->dirty_objects.clear();
		return;
	}

	for(auto handle : world->dirty_objects)
	{
		auto offset = (handle - 1) * sizeof(render_object_data);
		gpu_mem_copy(cmd, world->host_objects + offset, world->objects + offset, sizeof(render_object_data));
	}
	world->dirty_objects.clear();

	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_COMPUTE | GPU_STAGE_VERTEX_SHADER);
}

static void renderer_world_vis_prepare(GPUCommandBuffer& cmd)
{
	for(auto& view : world->views)
	{
		auto* cbuffer = reinterpret_cast<renderview_cbuffer*>(gpu_map_memory(view.cbuffer[renderer_gfx_frame_index()]));
		cbuffer->lod_base = 10.0f;
		if(view.is_shadow)
		{
			cbuffer->flags = view.flags | RENDER_VIEW_ORTHOGRAPHIC;
			cbuffer->lod_step = 1.5f;
		}
		else
		{
			cbuffer->flags = view.flags;
			cbuffer->lod_step = 3.5f;
		}
		cbuffer->lod_bias = view.lod_bias;

		for(int i = 0; i < RENDER_BUCKET_COUNT; i++)
		{
			view.cluster_bucket_offsets[i] = 0u;
			for(int j = 0; j < i; j++)
				view.cluster_bucket_offsets[i] += view.cluster_bucket_sizes[j];

			uvec2* bucket = reinterpret_cast<uvec2*>(gpu_map_memory(view.buckets[renderer_gfx_frame_index()])) + i;
			*bucket = {view.cluster_bucket_offsets[i], 0};
		}

		gpu_mem_clear(cmd, view.clusters, sizeof(uvec2) * view.primitive_capacity);
		gpu_mem_clear(cmd, view.visibility, sizeof(u32) * view.primitive_capacity);
	}
}

static void renderer_world_viscull_instances(GPUCommandBuffer& cmd)
{
	struct InstanceCullCSData
	{
		GPUDevicePointer instances;
		GPUDevicePointer clusters;
		GPUDevicePointer buckets;
		GPUDevicePointer objects;
		GPUDevicePointer lods;
		u32 count;
	} shader_data;

	shader_data.objects = gpu_host_to_device_pointer(world->objects);
	shader_data.lods = gpu_host_to_device_pointer(renderer_geometry_get_storage().lod);
	
	gpu_set_pipeline(cmd, instance_cull_cs);
	for(auto& view : world->views)
	{
		if(!view.instance_count)
			continue;

		shader_data.instances = gpu_host_to_device_pointer(view.instances);
		shader_data.clusters = gpu_host_to_device_pointer(view.clusters);
		shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
		shader_data.count = view.instance_count;

		gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]);
		gpu_dispatch(cmd, &shader_data, {(view.instance_count + 31u) / 32u, 1u, 1u});
	}
}

static void renderer_world_viscull_clusters(GPUCommandBuffer& cmd)
{
	struct ClusterCullCSData
	{
		GPUDevicePointer cluster_instances;
		GPUDevicePointer visibility;
		GPUDevicePointer buckets;
		GPUDevicePointer objects;
		GPUDevicePointer clusters;
		u32 count;
	} shader_data;

	shader_data.objects = gpu_host_to_device_pointer(world->objects);
	shader_data.clusters = gpu_host_to_device_pointer(renderer_geometry_get_storage().cluster);
	
	gpu_set_pipeline(cmd, cluster_cull_cs);
	for(auto& view : world->views)
	{
		auto size = view.cluster_bucket_offsets[RENDER_BUCKET_COUNT - 1] + view.cluster_bucket_sizes[RENDER_BUCKET_COUNT - 1];
		if(!size)
			continue;

		shader_data.cluster_instances = gpu_host_to_device_pointer(view.clusters);
		shader_data.visibility = gpu_host_to_device_pointer(view.visibility);
		shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
		shader_data.count = size;

		gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]);
		gpu_dispatch(cmd, &shader_data, {(size + 31u) / 32u, 1u, 1u});
	}
}

static void renderer_world_compact_drawcalls(GPUCommandBuffer& cmd)
{
	gpu_set_pipeline(cmd, ps_index_cs);

	struct PSIndexData
	{
		GPUDevicePointer input;
		GPUDevicePointer output;
		GPUDevicePointer partial;
		u32 count;
	} ps_index_data;

	struct PSPartialData
	{
		GPUDevicePointer input;
		GPUDevicePointer output;
		u32 count;
	} ps_partial_data;

	for(auto& view : world->views)
	{
		auto size = view.cluster_bucket_offsets[RENDER_BUCKET_COUNT - 1] + view.cluster_bucket_sizes[RENDER_BUCKET_COUNT - 1];
		if(!size)
			continue;

		ps_index_data.input = gpu_host_to_device_pointer(view.visibility);
		ps_index_data.output = gpu_host_to_device_pointer(view.visibility_sums[0]);
		ps_index_data.partial = gpu_host_to_device_pointer(view.visibility_sums[1]);
		ps_index_data.count = size;
		view.intermediate_sizes[0] = size;
		size = (size / 512u) + 1u;
		gpu_dispatch(cmd, &ps_index_data, {size, 1u, 1u});
	}
	
	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

	for(auto& view : world->views)
	{
		auto size = (view.intermediate_sizes[0] / 512u) + 1u;
		for(u32 i = 1; i < view.visibility_sums.size() - 1; i++)
		{
			gpu_set_pipeline(cmd, ps_index_cs);

			ps_index_data.input = gpu_host_to_device_pointer(view.visibility_sums[i]);
			ps_index_data.output = gpu_host_to_device_pointer(view.visibility_sums[i]);
			ps_index_data.partial = gpu_host_to_device_pointer(view.visibility_sums[i + 1]);
			ps_index_data.count = size;
			view.intermediate_sizes[i] = size;
			size = (size / 512u) + 1;
			gpu_dispatch(cmd, &ps_index_data, {size, 1u, 1u});

			gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

			gpu_set_pipeline(cmd, ps_partial_cs);
			ps_partial_data.input = gpu_host_to_device_pointer(view.visibility_sums[i]);
			ps_partial_data.output = gpu_host_to_device_pointer(view.visibility_sums[i - 1]);
			ps_partial_data.count = view.intermediate_sizes[i - 1];
			gpu_dispatch(cmd, &ps_partial_data, {view.intermediate_sizes[i], 1u, 1u});
			gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);
		}
	}
}

static void renderer_world_generate_drawcalls(GPUCommandBuffer& cmd)
{
	struct CMDGenCSData
	{
		GPUDevicePointer cluster_instances;
		GPUDevicePointer objects;
		GPUDevicePointer visibility;
		GPUDevicePointer visibility_prefixsum;
		GPUDevicePointer buckets;
		GPUDevicePointer commands;
		GPUDevicePointer clusters;
		u32 count;
		int is_visbuffer;
	} shader_data;

	shader_data.objects = gpu_host_to_device_pointer(world->objects);
	shader_data.clusters = gpu_host_to_device_pointer(renderer_geometry_get_storage().cluster);

	gpu_set_pipeline(cmd, cmdgen_cs);
	for(auto& view : world->views)
	{
		auto size = view.cluster_bucket_offsets[RENDER_BUCKET_COUNT - 1] + view.cluster_bucket_sizes[RENDER_BUCKET_COUNT - 1];
		if(!size)
			continue;

		shader_data.cluster_instances = gpu_host_to_device_pointer(view.clusters);
		shader_data.visibility = gpu_host_to_device_pointer(view.visibility);
		shader_data.visibility_prefixsum = gpu_host_to_device_pointer(view.visibility_sums[0]);
		shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
		shader_data.commands = gpu_host_to_device_pointer(view.commands);
		shader_data.count = size;
		shader_data.is_visbuffer = !view.is_shadow;

		gpu_dispatch(cmd, &shader_data, {(size / 256u) + 1u, 1u, 1u});
	}
}

void renderer_world_determine_visibility(GPUCommandBuffer& cmd)
{
	ZoneScopedN("r_viscull");

	renderer_world_vis_prepare(cmd);
	renderer_world_viscull_instances(cmd);
	gpu_barrier(cmd, GPU_STAGE_TRANSFER | GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);
	
	renderer_world_viscull_clusters(cmd);
	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);
	
	renderer_world_compact_drawcalls(cmd);
	renderer_world_generate_drawcalls(cmd);
	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMMAND_PROCESSOR, GPU_HAZARD_INDIRECT_ARGS);
}

GPUPointer renderer_world_get_objects()
{
	return world->objects;
}

render_bucket_draw renderer_world_get_drawcall(renderViewID id, render_bucket bucket)
{
	assert(id);
	auto& view = world->views[id - 1];

	return 
	{
		view.commands + (view.cluster_bucket_offsets[bucket] * sizeof(GPUIndexedIndirectCommand)),
		view.buckets[renderer_gfx_frame_index()] + (bucket * sizeof(uvec2) + sizeof(u32)),
		view.clusters,
		view.cluster_bucket_sizes[bucket]
	};
}

}
