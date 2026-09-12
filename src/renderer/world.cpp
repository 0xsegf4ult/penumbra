#include <renderer/world.hpp>
#include <renderer/resource.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/resource.hpp>
#include <penumbra/config.hpp>
#include <penumbra/cvar.hpp>
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

enum renderview_flags
{
	RENDER_VIEW_FRUSTUM_CULL = 0x1,
	RENDER_VIEW_CONE_CULL = 0x2,
	RENDER_VIEW_CONTRIBUTION_CULL = 0x4,
	RENDER_VIEW_OCCLUSION_CULL = 0x8,
	RENDER_VIEW_ORTHOGRAPHIC = 0x10,
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
	float projX;
	float projY;
	float min_contrib;
	u32 flags;
	u32 lod_bias;
};

struct render_view
{
	u32 primitive_capacity = 65536u;

	GPUPointer visible_meshlets;
	GPUPointer counters;
	GPUPointer dispatch_args;
	
	GPUPointer meshlet_instances;
	GPUPointer indirect_commands;
	GPUPointer bucket_counters; 

	GPUPointer buckets[config::renderer_frames_in_flight];
	GPUPointer cbuffer[config::renderer_frames_in_flight];

	u32 flags;
	u32 lod_bias{0};
	bool is_shadow;
	bool freeze_culling{false};
};

enum renderobject_flags
{
	RENDER_OBJECT_DISABLED = 1
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

struct GPUWorldMeshlet
{
	u32 renderable_index;
	u32 bucket;
	u32 lod;
	u32 meshlet_index;
};

struct GPUWorldMeshletInstance
{
	u32 renderable_index;
	u32 meshlet_index;
};

struct GPUWorldCounters
{
	u32 visible_meshlets;
	u32 threadgroup_count;
};

struct render_world
{
	u32 object_capacity = 16384u;
	u32 object_count = 0u;

	std::vector<u64> id_alloc_bitmap;

	GPUPointer host_objects;
	GPUPointer objects;

	std::vector<renderObjectID> dirty_objects;
	std::vector<render_view> views;
	
	u32 bucket_sizes[RENDER_BUCKET_COUNT];
	u32 bucket_offsets[RENDER_BUCKET_COUNT];

	//FIXME: sparse set might be better?
	std::map<renderObjectID, renderer_skinned_geometry_instance> sg_instances;
};

static render_world* world = nullptr;;

static GPUPipeline skinning_cs;
static GPUPipeline vis_phase1_cs;
static GPUPipeline vis_phase2_cs;

static void set_fcull_cv(cvar_t* cvar)
{
	if(cvar->int_v)
		world->views[0].flags |= RENDER_VIEW_FRUSTUM_CULL;
       	else
		world->views[0].flags &= ~RENDER_VIEW_FRUSTUM_CULL;
}	

static cvar_t fcull_cv
{
	.name = "r_frustumcull",
	.type = CVAR_TYPE_INT,
	.int_defv = 1,
	.int_v = 1,
	.callback = set_fcull_cv
};

static void set_ctcull_cv(cvar_t* cvar)
{
	if(cvar->int_v)
		world->views[0].flags |= RENDER_VIEW_CONTRIBUTION_CULL;
	else
		world->views[0].flags &= ~RENDER_VIEW_CONTRIBUTION_CULL;
}

static cvar_t ctcull_cv
{
	.name = "r_contribcull",
	.type = CVAR_TYPE_INT,
	.int_defv = 1,
	.int_v = 1,
	.callback = set_ctcull_cv
};

static void set_frcull_cv(cvar_t* cvar)
{
	world->views[0].freeze_culling = (cvar->int_v > 0);
}

static cvar_t frcull_cv
{
	.name = "r_freezeculling",
	.type = CVAR_TYPE_INT,
	.int_defv = 0,
	.int_v = 0,
	.callback = set_frcull_cv
};

void renderer_world_init()
{
	world = new render_world();

	cvar_register(&fcull_cv);
	cvar_register(&frcull_cv);
	cvar_register(&ctcull_cv);

	skinning_cs = gpu_create_compute_pipeline(load_shader("shaders/geometry_skinning"));
	vis_phase1_cs = gpu_create_compute_pipeline(load_shader("shaders/vis_phase1"));
	vis_phase2_cs = gpu_create_compute_pipeline(load_shader("shaders/vis_phase2"));

	world->host_objects = gpu_allocate_memory(sizeof(render_object_data) * world->object_capacity, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	world->objects = gpu_allocate_memory(sizeof(render_object_data) * world->object_capacity);

	world->id_alloc_bitmap.resize(world->object_capacity / 64);
	for(auto& word : world->id_alloc_bitmap)
		word = ~(0ull);
}

static void renderer_destroy_view(render_view& view)
{
	for(auto& elem : view.cbuffer)
		gpu_free_memory(elem);

	for(auto& elem : view.buckets)
		gpu_free_memory(elem);
	
	gpu_free_memory(view.bucket_counters);
	gpu_free_memory(view.indirect_commands);
	gpu_free_memory(view.dispatch_args);
	gpu_free_memory(view.counters);
	gpu_free_memory(view.visible_meshlets);
	gpu_free_memory(view.meshlet_instances);
}

void renderer_world_cleanup()
{
	for(auto& view : world->views)
		renderer_destroy_view(view);

	gpu_free_memory(world->objects);
	gpu_free_memory(world->host_objects);
	
	delete world;
	
	gpu_destroy_pipeline(vis_phase2_cs);
	gpu_destroy_pipeline(vis_phase1_cs);
	gpu_destroy_pipeline(skinning_cs);
}

renderViewID renderer_create_view(const render_view_desc& desc)
{
	world->views.push_back(render_view{});
	auto& view = world->views.back();

	view.meshlet_instances = gpu_allocate_memory(sizeof(GPUWorldMeshletInstance) * view.primitive_capacity);
	view.visible_meshlets = gpu_allocate_memory(sizeof(GPUWorldMeshlet) * view.primitive_capacity);
	view.dispatch_args = gpu_allocate_memory(sizeof(uvec3), GPU_MEMORY_PRIVATE, GPU_BUFFER_INDIRECT);
	view.counters = gpu_allocate_memory(sizeof(GPUWorldCounters));
	view.bucket_counters = gpu_allocate_memory(sizeof(u32) * RENDER_BUCKET_COUNT, GPU_MEMORY_PRIVATE, GPU_BUFFER_INDIRECT);
	view.indirect_commands = gpu_allocate_memory(sizeof(GPUIndexedIndirectCommand) * view.primitive_capacity, GPU_MEMORY_PRIVATE, GPU_BUFFER_INDIRECT);


	for(int i = 0; i < config::renderer_frames_in_flight; i++)
	{
		view.cbuffer[i] = gpu_allocate_memory(sizeof(renderview_cbuffer), GPU_MEMORY_MAPPED, GPU_BUFFER_UNIFORM);
		view.buckets[i] = gpu_allocate_memory(sizeof(u32) * RENDER_BUCKET_COUNT, GPU_MEMORY_MAPPED);
	}

	view.is_shadow = desc.is_shadow;
	view.flags = RENDER_VIEW_FRUSTUM_CULL | RENDER_VIEW_CONTRIBUTION_CULL;
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
	cbuffer->projX = cam.proj[0][0];
	cbuffer->projY = cam.proj[1][1];
	cbuffer->min_contrib = 0.01f;
	if(id == 2)
		cbuffer->min_contrib = 0.005f;
	if(id == 3)
		cbuffer->min_contrib = 0.0005f;
	if(id == 4)
		cbuffer->min_contrib = 0.0001f;
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

renderObjectID renderer_world_insert_object(const render_object_desc& desc)
{
	ZoneScoped;

	u32 index = 0;

	for(size_t i = 0; i < world->id_alloc_bitmap.size(); i++)
	{
		u64 word = world->id_alloc_bitmap[i];
		if(word == 0)
			continue;

		int bit = __builtin_ctzll(word);
		world->id_alloc_bitmap[i] &= ~(1ull << bit);
		index = (i * 64 + bit) + 1;
		break;
	}

	if(!index)
	{
		log::warn("render_world: object storage capacity [{}] exceeded", world->object_capacity);
		return renderObjectID{0};
	}
	
	if(index > world->object_count)
		world->object_count = index;

	render_object_data* obj = reinterpret_cast<render_object_data*>(gpu_map_memory(world->host_objects)) + (index - 1);
	
	renderObjectID handle{index};
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
	obj->flags = 0;

	world->bucket_sizes[bucket] += geom_data.l0_cluster_count;

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

void renderer_world_set_visible(renderObjectID object, bool visible)
{
	assert(object);
	
	auto* data = reinterpret_cast<render_object_data*>(gpu_map_memory(world->host_objects)) + (object - 1);
	if(visible)
		data->flags &= (~RENDER_OBJECT_DISABLED);
	else
		data->flags |= RENDER_OBJECT_DISABLED;

	world->dirty_objects.push_back(object);
}

void renderer_world_remove_object(renderObjectID object)
{
	renderer_world_set_visible(object, false);

	if(object == world->object_count)
		world->object_count--;

	u32 raw = object - 1;
	u32 map_offset = raw / 64;
	u32 bit_offset = raw % 64;
	world->id_alloc_bitmap[map_offset] |= (1ull << bit_offset);
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
	ZoneScoped;

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
	for(int i = 0; i < RENDER_BUCKET_COUNT; i++)
	{
		world->bucket_offsets[i] = 0u;
		for(int j = 0; j < i; j++)
			world->bucket_offsets[i] += world->bucket_sizes[j];
	}

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
			*(reinterpret_cast<u32*>(gpu_map_memory(view.buckets[renderer_gfx_frame_index()])) + i) = world->bucket_offsets[i];
		}

		gpu_mem_clear(cmd, view.counters, sizeof(GPUWorldCounters));
		gpu_mem_clear(cmd, view.bucket_counters, sizeof(u32) * RENDER_BUCKET_COUNT);
	}
}

static void renderer_world_vis_phase1(GPUCommandBuffer& cmd)
{
	gpu_set_pipeline(cmd, vis_phase1_cs);

	struct VisP1Data
	{
		GPUDevicePointer renderables;
		GPUDevicePointer lods;
		GPUDevicePointer counters;
		GPUDevicePointer visible_meshlets;
		GPUDevicePointer dispatch_args;
		u32 instance_count;
	} shader_data;

	shader_data.renderables = gpu_host_to_device_pointer(world->objects);
	shader_data.lods = gpu_host_to_device_pointer(renderer_geometry_get_storage().lod);
	shader_data.instance_count = world->object_count;

	for(auto& view : world->views)
	{
		shader_data.counters = gpu_host_to_device_pointer(view.counters);
		shader_data.visible_meshlets = gpu_host_to_device_pointer(view.visible_meshlets);
		shader_data.dispatch_args = gpu_host_to_device_pointer(view.dispatch_args);

		gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]);
		gpu_dispatch(cmd, &shader_data, {(world->object_count + 63u) / 64u, 1u, 1u}); 
	}
}

static void renderer_world_vis_phase2(GPUCommandBuffer& cmd)
{
	gpu_set_pipeline(cmd, vis_phase2_cs);

	struct VisP2Data
	{
		GPUDevicePointer renderables;
		GPUDevicePointer clusters;
		GPUDevicePointer counters;
		GPUDevicePointer visible_meshlets;
		GPUDevicePointer meshlet_instances;
		GPUDevicePointer buckets;
		GPUDevicePointer bucket_counters;
		GPUDevicePointer indirect_args;
	} shader_data;

	shader_data.renderables = gpu_host_to_device_pointer(world->objects);
	shader_data.clusters = gpu_host_to_device_pointer(renderer_geometry_get_storage().cluster);

	for(auto& view : world->views)
	{
		shader_data.counters = gpu_host_to_device_pointer(view.counters);
		shader_data.visible_meshlets = gpu_host_to_device_pointer(view.visible_meshlets);
		shader_data.meshlet_instances = gpu_host_to_device_pointer(view.meshlet_instances);
		shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
		shader_data.bucket_counters = gpu_host_to_device_pointer(view.bucket_counters);
		shader_data.indirect_args = gpu_host_to_device_pointer(view.indirect_commands);

		gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]);
		gpu_dispatch_indirect(cmd, &shader_data, view.dispatch_args);
	}
}

void renderer_world_determine_visibility(GPUCommandBuffer& cmd)
{
	ZoneScopedN("r_viscull");

	renderer_world_vis_prepare(cmd);
	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_COMPUTE);

	renderer_world_vis_phase1(cmd);
	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE | GPU_STAGE_COMMAND_PROCESSOR, GPU_HAZARD_MEMORY | GPU_HAZARD_INDIRECT_ARGS);

	renderer_world_vis_phase2(cmd);
	gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMMAND_PROCESSOR | GPU_STAGE_VERTEX_SHADER | GPU_STAGE_COMPUTE, GPU_HAZARD_MEMORY | GPU_HAZARD_INDIRECT_ARGS);
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
		view.indirect_commands + (bucket * sizeof(GPUIndirectCommand)),
		view.meshlet_instances,
	};
}

render_bucket_mdi_draw renderer_world_get_mdi_drawcall(renderViewID id, render_bucket bucket)
{
	assert(id);
	auto& view = world->views[id - 1];

	return
	{
		view.indirect_commands + (world->bucket_offsets[bucket] * sizeof(GPUIndexedIndirectCommand)),
		view.bucket_counters + (bucket * sizeof(u32)),
		view.meshlet_instances,
		world->bucket_sizes[bucket]
	};
}

}
