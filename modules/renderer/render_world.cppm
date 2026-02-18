module;

#include <cassert>

export module penumbra.renderer:render_world;

import :api;

import penumbra.core;
import penumbra.math;
import penumbra.gpu;
import std;

using std::uint32_t;

namespace penumbra
{

struct RenderViewCBuffer
{
	mat4 viewmat;
	vec4 cam_pos;
	float lod_step;
	float lod_base;
};

struct RenderViewData
{
	GPUPointer instances;
	GPUPointer clusters;
	GPUPointer buckets;
	GPUPointer visibility;
	GPUPointer commands;
	
	std::array<GPUPointer, config::renderer_frames_in_flight> cbuffer;
	uint32_t instance_count{0};
	uint32_t instance_out_head{0};

	std::vector<uint32_t> cluster_bucket_sizes;
	std::vector<uint32_t> cluster_bucket_offsets;
};

struct RenderObjectData
{
	mat4 transform;
	vec4 sphere;
	uint32_t geom_lod_offset;
	uint32_t pack_bucket_lod_count;
};

export class RenderWorld
{
public:
	void init()
	{
		host_objects = gpu_allocate_memory(sizeof(RenderObjectData) * object_capacity, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
		objects = gpu_allocate_memory(sizeof(RenderObjectData) * object_capacity);
		instance_cull_cs = gpu_create_compute_pipeline(*load_shader("shaders/instance_cull"));
		cluster_cull_cs = gpu_create_compute_pipeline(*load_shader("shaders/cluster_cull"));
		cmdgen_vb_cs = gpu_create_compute_pipeline(*load_shader("shaders/generate_commands_vb"));
	}	

	~RenderWorld()
	{
		gpu_destroy_pipeline(cmdgen_vb_cs);
		gpu_destroy_pipeline(cluster_cull_cs);
		gpu_destroy_pipeline(instance_cull_cs);
		for(auto& view : views)
		{
			for(auto& elem : view.cbuffer)
				gpu_free_memory(elem);

			gpu_free_memory(view.commands);
			gpu_free_memory(view.visibility);
			gpu_free_memory(view.buckets);
			gpu_free_memory(view.clusters);
			gpu_free_memory(view.instances);
		}

		gpu_free_memory(objects);
		gpu_free_memory(host_objects);
	}

	RenderView register_view()
	{
		views.push_back(RenderViewData{});
		
		auto& view = views.back();
		view.instances = gpu_allocate_memory(sizeof(uvec2) * object_capacity, GPU_MEMORY_MAPPED); 
		view.clusters = gpu_allocate_memory(sizeof(uvec2) * 65536);
		view.buckets = gpu_allocate_memory(sizeof(uvec2) * RENDER_BUCKET_COUNT, GPU_MEMORY_MAPPED, GPU_BUFFER_INDIRECT);
		view.visibility = gpu_allocate_memory(sizeof(uint32_t) * 65536);
		view.commands = gpu_allocate_memory(sizeof(GPUIndirectCommand) * 65536, GPU_MEMORY_PRIVATE, GPU_BUFFER_INDIRECT);
		for(int i = 0; i < config::renderer_frames_in_flight; i++)
			view.cbuffer[i] = gpu_allocate_memory(sizeof(RenderViewCBuffer), GPU_MEMORY_MAPPED, GPU_BUFFER_UNIFORM);

		view.cluster_bucket_sizes.resize(RENDER_BUCKET_COUNT);
		view.cluster_bucket_offsets.resize(RENDER_BUCKET_COUNT);

		return RenderView{static_cast<uint32_t>(views.size())};
	}

	void update_view_camera(RenderView render_view, mat4 view, mat4 proj)
	{
		assert(render_view);
		
		auto& rv = views[render_view - 1];
		auto* cbuffer = reinterpret_cast<RenderViewCBuffer*>(gpu_map_memory(rv.cbuffer[renderer_gfx_frame_index()]));
		cbuffer->viewmat = view;
	}

	RenderObject insert_object(const RenderObjectDescription& data, array_proxy<RenderView> view_list)
	{
		if(object_count >= object_capacity)
		{
			log::warn("render_world: object storage capacity ({}) exceeded", object_capacity);
			return RenderObject{0};
		}

		RenderObjectData* obj = reinterpret_cast<RenderObjectData*>(gpu_map_memory(host_objects)) + object_count;

		obj->transform = data.transform.as_matrix();
		obj->geom_lod_offset = data.geom_lod_offset;
		obj->pack_bucket_lod_count = (data.bucket << 16) | data.geom_lod_count;
		
		object_count++;
		RenderObject handle{object_count};
		dirty_objects.push_back(handle);
		
		for(auto view_handle : view_list)
		{
			if(!view_handle)
				continue;

			RenderViewData& view = views[view_handle - 1];
			auto bucket_offset = std::to_underlying(data.bucket);

			auto* object_instance = reinterpret_cast<uvec2*>(gpu_map_memory(view.instances)) + view.instance_count;
			*object_instance = {handle, view.cluster_bucket_sizes[bucket_offset]};
			view.instance_count++;

			view.cluster_bucket_sizes[bucket_offset] += data.geom_l0_cluster_count;
		}

		return handle;
	}

	void upload_objects(GPUCommandBuffer& cmd)
	{
		if(dirty_objects.empty())
			return;

		if(dirty_objects.size() == object_count)
		{
			log::debug("render_world: reuploading object buffer");
			gpu_mem_copy(cmd, host_objects, objects, object_count * sizeof(RenderObjectData));
			dirty_objects.clear();
			return;
		}

		uint32_t prev_handle = 0;
		uint32_t first_handle = 0;
		uint32_t range_size = 0;

		for(auto handle : dirty_objects)
		{
			if(prev_handle + 1 == handle)
			{
				range_size++;
				prev_handle = handle;
				continue;
			}

			if(first_handle)
			{
				auto offset = (first_handle - 1) * sizeof(RenderObjectData);
				auto size = range_size * sizeof(RenderObjectData);
				gpu_mem_copy(cmd, host_objects + offset, objects + offset, size);
			}

			first_handle = handle;
			prev_handle = handle;
			range_size = 0;
		}
		dirty_objects.clear();

		gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_COMPUTE | GPU_STAGE_VERTEX_SHADER);
	}

	void determine_visibility(GPUCommandBuffer& cmd)
	{
		for(auto& view : views)
		{
			auto* cbuffer = reinterpret_cast<RenderViewCBuffer*>(gpu_map_memory(view.cbuffer[renderer_gfx_frame_index()]));
			cbuffer->cam_pos = vec4{0.0f, 0.0f, 0.0f, 1.0f};
			cbuffer->lod_base = 10.0f;
			cbuffer->lod_step = 1.5f;

			for(int i = 0; i < RENDER_BUCKET_COUNT; i++)
			{
				view.cluster_bucket_offsets[i] = 0u;
				for(int j = 0; j < i; j++)
					view.cluster_bucket_offsets[i] += view.cluster_bucket_sizes[j];

				uvec2* bucket = reinterpret_cast<uvec2*>(gpu_map_memory(view.buckets)) + i;
				*bucket = {view.cluster_bucket_offsets[i], 0};
			}

			gpu_mem_clear(cmd, view.clusters, sizeof(uvec2) * 65536);
			gpu_mem_clear(cmd, view.visibility, sizeof(uint32_t) * 65536);
		}
		
		gpu_set_pipeline(cmd, instance_cull_cs);

		for(auto& view : views)
		{
			if(!view.instance_count)
				continue;

			struct InstanceCullCSData
			{
				GPUDevicePointer instances;
				GPUDevicePointer clusters;
				GPUDevicePointer buckets;
				GPUDevicePointer objects;
				GPUDevicePointer lods;
				uint32_t count;	
			} shader_data;

			shader_data.instances = gpu_host_to_device_pointer(view.instances);
			shader_data.clusters = gpu_host_to_device_pointer(view.clusters);
			shader_data.buckets = gpu_host_to_device_pointer(view.buckets);
			shader_data.objects = gpu_host_to_device_pointer(objects);
			shader_data.lods = renderer_geometry_lod_device_pointer();
			shader_data.count = view.instance_count;

			gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]); 
			gpu_dispatch(cmd, &shader_data, {(view.instance_count + 31u) / 32u, 1u, 1u});
		}		
		
		gpu_barrier(cmd, GPU_STAGE_TRANSFER | GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

		gpu_set_pipeline(cmd, cluster_cull_cs);

		for(auto& view : views)
		{
			auto size = view.cluster_bucket_offsets.back() + view.cluster_bucket_sizes.back();
			if(!size)
				continue;

			struct ClusterCullCSData
			{
				GPUDevicePointer cluster_instances;
				GPUDevicePointer visibility;
				GPUDevicePointer buckets;
				GPUDevicePointer objects;
				GPUDevicePointer clusters;
				uint32_t count;
			} shader_data;
			
			shader_data.cluster_instances = gpu_host_to_device_pointer(view.clusters);
			shader_data.visibility = gpu_host_to_device_pointer(view.visibility);
			shader_data.buckets = gpu_host_to_device_pointer(view.buckets);
			shader_data.objects = gpu_host_to_device_pointer(objects);
			shader_data.clusters = renderer_geometry_cluster_device_pointer();
			shader_data.count = size;
			
			gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]); 
			gpu_dispatch(cmd, &shader_data, {(size + 31u) / 32u, 1u, 1u});
		}

		gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

		gpu_set_pipeline(cmd, cmdgen_vb_cs);

		for(auto& view : views)
		{
			auto size = view.cluster_bucket_offsets.back() + view.cluster_bucket_sizes.back();
			if(!size)
				continue;

			struct CMDGenCSData
			{
				GPUDevicePointer cluster_instances;
				GPUDevicePointer visibility;
				GPUDevicePointer commands;
				GPUDevicePointer clusters;
				uint32_t count;
			} shader_data;

			shader_data.cluster_instances = gpu_host_to_device_pointer(view.clusters);
			shader_data.visibility = gpu_host_to_device_pointer(view.visibility);
			shader_data.commands = gpu_host_to_device_pointer(view.commands);
			shader_data.clusters = renderer_geometry_cluster_device_pointer();
			shader_data.count = size;

			gpu_dispatch(cmd, &shader_data, {(size / 256u) + 1u, 1u, 1u});
		}

		gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMMAND_PROCESSOR, GPU_HAZARD_INDIRECT_ARGS);
	}

	GPUDevicePointer get_objects()
	{
		return gpu_host_to_device_pointer(objects);
	}

	RenderBucketData get_bucket(RenderView view_handle, RenderBucket bucket)	
	{
		assert(view_handle);
		auto& view = views[view_handle - 1];
		
		return
		{
			view.commands + (view.cluster_bucket_offsets[bucket] * sizeof(GPUIndirectCommand)),
			view.buckets + (bucket * sizeof(uvec2) + sizeof(uint32_t)),
				
			view.clusters,
			view.cluster_bucket_sizes[bucket]
		};
	}		
private:
	std::vector<RenderViewData> views;
	std::vector<RenderObject> dirty_objects;

	uint32_t object_capacity = 16384u;
	uint32_t object_count = 0;

	GPUPointer host_objects;
	GPUPointer objects;

	GPUPipeline instance_cull_cs;
	GPUPipeline cluster_cull_cs;
	GPUPipeline cmdgen_vb_cs;
};

}
