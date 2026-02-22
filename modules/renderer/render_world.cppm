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
	vec4 frustum_planes[4];
	float lod_step;
	float lod_base;
	float znear;
	float zfar;
	uint32_t flags;	
};

struct RenderViewData
{
	GPUPointer instances;
	GPUPointer clusters;
	std::array<GPUPointer, config::renderer_frames_in_flight> buckets;
	GPUPointer visibility;
	GPUPointer commands;

	std::vector<GPUPointer> visibility_sums;
	std::array<uint32_t, 16> intermediate_sizes;

	std::array<GPUPointer, config::renderer_frames_in_flight> cbuffer;
	uint32_t instance_count{0};

	std::vector<uint32_t> cluster_bucket_sizes;
	std::vector<uint32_t> cluster_bucket_offsets;
};

struct RenderObjectData
{
	mat4 transform;
	vec4 sphere;
	uint32_t material_offset;
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
		instance_cull_cs = gpu_create_compute_pipeline(load_shader("shaders/instance_cull"));
		cluster_cull_cs = gpu_create_compute_pipeline(load_shader("shaders/cluster_cull"));
		cmdgen_vb_cs = gpu_create_compute_pipeline(load_shader("shaders/generate_commands_vb"));
		ps_index_cs = gpu_create_compute_pipeline(load_shader("shaders/prefix_scan_index"));
		ps_partial_cs = gpu_create_compute_pipeline(load_shader("shaders/prefix_scan_add_partial"));
	}	

	~RenderWorld()
	{
		gpu_destroy_pipeline(ps_partial_cs);
		gpu_destroy_pipeline(ps_index_cs);
		gpu_destroy_pipeline(cmdgen_vb_cs);
		gpu_destroy_pipeline(cluster_cull_cs);
		gpu_destroy_pipeline(instance_cull_cs);
		for(auto& view : views)
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

		gpu_free_memory(objects);
		gpu_free_memory(host_objects);
	}

	RenderView register_view()
	{
		views.push_back(RenderViewData{});
		
		auto& view = views.back();
		view.instances = gpu_allocate_memory(sizeof(uvec2) * object_capacity, GPU_MEMORY_MAPPED); 
		view.clusters = gpu_allocate_memory(sizeof(uvec2) * 65536);
		view.visibility = gpu_allocate_memory(sizeof(uint32_t) * 65536);
		view.commands = gpu_allocate_memory(sizeof(GPUIndirectCommand) * 65536, GPU_MEMORY_PRIVATE, GPU_BUFFER_INDIRECT);
		for(int i = 0; i < config::renderer_frames_in_flight; i++)
		{
			view.cbuffer[i] = gpu_allocate_memory(sizeof(RenderViewCBuffer), GPU_MEMORY_MAPPED, GPU_BUFFER_UNIFORM);
			view.buckets[i] = gpu_allocate_memory(sizeof(uvec2) * RENDER_BUCKET_COUNT, GPU_MEMORY_MAPPED, GPU_BUFFER_INDIRECT);
		}

		view.cluster_bucket_sizes.resize(RENDER_BUCKET_COUNT);
		view.cluster_bucket_offsets.resize(RENDER_BUCKET_COUNT);

		view.visibility_sums.push_back(gpu_allocate_memory(sizeof(uint32_t) * 65536));
		uint32_t n = (65536 / 512) + 1u;
		while(n > 1)
		{
			view.visibility_sums.push_back(gpu_allocate_memory(sizeof(uint32_t) * n));
			n = (n / 512) + 1u;
		}

		view.visibility_sums.push_back(gpu_allocate_memory(sizeof(uint32_t)));

		return RenderView{static_cast<uint32_t>(views.size())};
	}

	void update_view_camera(RenderView render_view, mat4 view, mat4 proj, vec3 pos)
	{
		assert(render_view);
		
		auto& rv = views[render_view - 1];
		auto* cbuffer = reinterpret_cast<RenderViewCBuffer*>(gpu_map_memory(rv.cbuffer[renderer_gfx_frame_index()]));
		cbuffer->viewmat = view;
		cbuffer->cam_pos = vec4{pos, 1.0f};

		mat4 projT = mat4::transpose(proj);
		const vec4 frustumX = Plane(projT[3] + projT[0]).normalize().as_vector();
		const vec4 frustumY = Plane(projT[3] + projT[1]).normalize().as_vector();
		cbuffer->frustum_planes[0] = vec4{frustumX.x, frustumX.z, frustumY.y, frustumY.z};

		cbuffer->znear = 0.1f;
		cbuffer->zfar = 128.0f;
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
		obj->sphere = data.sphere;
		obj->material_offset = data.material_offset;
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
			cbuffer->lod_base = 10.0f;
			cbuffer->lod_step = 3.5f;
			cbuffer->flags = 1;

			for(int i = 0; i < RENDER_BUCKET_COUNT; i++)
			{
				view.cluster_bucket_offsets[i] = 0u;
				for(int j = 0; j < i; j++)
					view.cluster_bucket_offsets[i] += view.cluster_bucket_sizes[j];

				uvec2* bucket = reinterpret_cast<uvec2*>(gpu_map_memory(view.buckets[renderer_gfx_frame_index()])) + i;
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
			shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
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
			shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
			shader_data.objects = gpu_host_to_device_pointer(objects);
			shader_data.clusters = renderer_geometry_cluster_device_pointer();
			shader_data.count = size;
			
			gpu_write_cbuffer_descriptor(cmd, view.cbuffer[renderer_gfx_frame_index()]); 
			gpu_dispatch(cmd, &shader_data, {(size + 31u) / 32u, 1u, 1u});
		}

		gpu_barrier(cmd, GPU_STAGE_COMPUTE, GPU_STAGE_COMPUTE);

		compact_drawcalls(cmd);

		gpu_set_pipeline(cmd, cmdgen_vb_cs);

		for(auto& view : views)
		{
			auto size = view.cluster_bucket_offsets.back() + view.cluster_bucket_sizes.back();
			if(!size)
				continue;

			struct CMDGenCSData
			{
				GPUDevicePointer cluster_instances;
				GPUDevicePointer objects;
				GPUDevicePointer visibility;
				GPUDevicePointer visibility_prefixsum;
				GPUDevicePointer buckets;
				GPUDevicePointer commands;
				GPUDevicePointer clusters;
				uint32_t count;
			} shader_data;

			shader_data.cluster_instances = gpu_host_to_device_pointer(view.clusters);
			shader_data.objects = gpu_host_to_device_pointer(objects);
			shader_data.visibility = gpu_host_to_device_pointer(view.visibility);
			shader_data.visibility_prefixsum = gpu_host_to_device_pointer(view.visibility_sums[0]);
			shader_data.buckets = gpu_host_to_device_pointer(view.buckets[renderer_gfx_frame_index()]);
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
			view.buckets[renderer_gfx_frame_index()] + (bucket * sizeof(uvec2) + sizeof(uint32_t)),
				
			view.clusters,
			view.cluster_bucket_sizes[bucket]
		};
	}		
private:
	void compact_drawcalls(GPUCommandBuffer& cmd)
	{
		gpu_set_pipeline(cmd, ps_index_cs);
			
		struct PSIndexData
		{
			GPUDevicePointer input;
			GPUDevicePointer output;
			GPUDevicePointer partial;
			uint32_t count;
		} ps_index_data;

		struct PSPartialData
		{
			GPUDevicePointer input;
			GPUDevicePointer output;
			uint32_t count;
		} ps_partial_data;

		for(auto& view : views)
		{
			auto size = view.cluster_bucket_offsets.back() + view.cluster_bucket_sizes.back();
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

		for(auto& view : views)
		{
			auto size = (view.intermediate_sizes[0] / 512u) + 1u;

			for(uint32_t i = 1 ; i < view.visibility_sums.size() - 1; i++)
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

	std::vector<RenderViewData> views;
	std::vector<RenderObject> dirty_objects;

	uint32_t object_capacity = 16384u;
	uint32_t object_count = 0;

	GPUPointer host_objects;
	GPUPointer objects;

	GPUPipeline instance_cull_cs;
	GPUPipeline cluster_cull_cs;
	GPUPipeline cmdgen_vb_cs;
	GPUPipeline ps_index_cs;
	GPUPipeline ps_partial_cs;
};

}
