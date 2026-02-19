module;

#include <cassert>

export module penumbra.renderer:geometry_buffer;

import penumbra.core;
import penumbra.math;
import penumbra.gpu;

import std;
using std::uint8_t, std::int32_t, std::uint32_t, std::size_t, std::memcpy;

namespace penumbra
{

export using geom_position_format = vec3;
export using geom_uv_format = vec2;
export using geom_nor_tan_format = uvec2;
export using geom_index_format = uint8_t; 

export struct geom_cluster_format
{
	int32_t vertex_offset;
	uint32_t vertex_count;
	uint32_t index_offset;
	uint32_t index_count;

	vec4 sphere;
	vec4 cone;
};

export struct geom_lod_format
{
	uint32_t cluster_offset{0u};
	uint32_t cluster_count{0u};
};

struct geometry_buffer_state
{
	GPUPointer host_vertex_pos;
	GPUPointer host_vertex_uv;
	GPUPointer host_vertex_nor_tan;
	GPUPointer host_indices;
	GPUPointer host_clusters;
	GPUPointer host_lods;
	
	GPUPointer vertex_pos;
	GPUPointer vertex_uv;
	GPUPointer vertex_nor_tan;
	GPUPointer indices;
	GPUPointer clusters;
	GPUPointer lods;

	uint32_t host_vertex_count{0};
	uint32_t host_index_count{0};
	uint32_t host_cluster_count{0};
	uint32_t host_lod_count{0};

	uint32_t resident_vertex_count{0};
	uint32_t resident_index_count{0};
	uint32_t resident_cluster_count{0};
	uint32_t resident_lod_count{0};
	
	uint32_t vertex_capacity{10000000u};
	uint32_t index_capacity{50000000u};
	uint32_t cluster_capacity{131072u};
	uint32_t lod_capacity{65536u};
};

geometry_buffer_state state;

export void renderer_geometry_init()
{
	state.host_vertex_pos = gpu_allocate_memory(state.vertex_capacity * sizeof(geom_position_format), GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	state.host_vertex_uv = gpu_allocate_memory(state.vertex_capacity * sizeof(geom_uv_format), GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	state.host_vertex_nor_tan = gpu_allocate_memory(state.vertex_capacity * sizeof(geom_nor_tan_format), GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	state.host_indices = gpu_allocate_memory(state.index_capacity * sizeof(geom_index_format), GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	state.host_clusters = gpu_allocate_memory(state.cluster_capacity * sizeof(geom_cluster_format), GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	state.host_lods = gpu_allocate_memory(state.lod_capacity * sizeof(geom_lod_format), GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
	
	state.vertex_pos = gpu_allocate_memory(state.vertex_capacity * sizeof(geom_position_format));
	state.vertex_uv = gpu_allocate_memory(state.vertex_capacity * sizeof(geom_uv_format));
	state.vertex_nor_tan = gpu_allocate_memory(state.vertex_capacity * sizeof(geom_nor_tan_format));
	state.indices = gpu_allocate_memory(state.index_capacity * sizeof(geom_index_format), GPU_MEMORY_PRIVATE, GPU_BUFFER_INDEX);
	state.clusters = gpu_allocate_memory(state.cluster_capacity * sizeof(geom_cluster_format));
	state.lods = gpu_allocate_memory(state.lod_capacity * sizeof(geom_lod_format));
}

export void renderer_geometry_shutdown()
{
	gpu_free_memory(state.lods);
	gpu_free_memory(state.clusters);
	gpu_free_memory(state.indices);
	gpu_free_memory(state.vertex_nor_tan);
	gpu_free_memory(state.vertex_uv);
	gpu_free_memory(state.vertex_pos);
	
	gpu_free_memory(state.host_lods);
	gpu_free_memory(state.host_clusters);
	gpu_free_memory(state.host_indices);
	gpu_free_memory(state.host_vertex_nor_tan);
	gpu_free_memory(state.host_vertex_uv);
	gpu_free_memory(state.host_vertex_pos);
}

export void renderer_geometry_clear()
{
	state.host_vertex_count = 0;
	state.host_index_count = 0;
	state.host_cluster_count = 0;
	state.host_lod_count = 0;

	state.resident_vertex_count = 0;
	state.resident_index_count = 0;
	state.resident_cluster_count = 0;
	state.resident_lod_count = 0;
}

export uint32_t renderer_geometry_push_vertices(const geom_position_format* pos_data, const geom_uv_format* uv_data, const geom_nor_tan_format* nrm_data, uint32_t count)
{
	auto offset = state.host_vertex_count;
	if(offset + count >= state.vertex_capacity)
		log::warn("renderer_geometry_buffer: out of vertex memory!");

	memcpy(gpu_map_memory(state.host_vertex_pos) + (offset * sizeof(geom_position_format)), pos_data, count * sizeof(geom_position_format));
	memcpy(gpu_map_memory(state.host_vertex_uv) + (offset * sizeof(geom_uv_format)), uv_data, count * sizeof(geom_uv_format));
	memcpy(gpu_map_memory(state.host_vertex_nor_tan) + (offset * sizeof(geom_nor_tan_format)), nrm_data, count * sizeof(geom_nor_tan_format));
	state.host_vertex_count += count;
	return offset;
}	

export uint32_t renderer_geometry_push_indices(const geom_index_format* data, uint32_t count)
{
	auto offset = state.host_index_count;
	if(offset + count >= state.index_capacity)
		log::warn("renderer_geometry_buffer: out of index memory!");

	memcpy(gpu_map_memory(state.host_indices) + (offset * sizeof(geom_index_format)), data, count * sizeof(geom_index_format));
	state.host_index_count += count;
	return offset;
}

export uint32_t renderer_geometry_push_clusters(const geom_cluster_format* data, uint32_t count)
{
	auto offset = state.host_cluster_count;
	if(offset + count >= state.cluster_capacity)
		log::warn("renderer_geometry_buffer: out of cluster memory!");
	
	memcpy(gpu_map_memory(state.host_clusters) + (offset * sizeof(geom_cluster_format)), data, count * sizeof(geom_cluster_format));
	state.host_cluster_count += count;
	return offset;
}

export uint32_t renderer_geometry_push_lods(const geom_lod_format* data, uint32_t count)
{
	auto offset = state.host_lod_count;
	if(offset + count >= state.lod_capacity)
		log::warn("renderer_geometry_buffer: out of LOD memory!");

	memcpy(gpu_map_memory(state.host_lods) + (offset * sizeof(geom_lod_format)), data, count * sizeof(geom_lod_format));
	state.host_lod_count += count;
	return offset;
}

export bool renderer_geometry_needs_upload()
{
	return (state.host_vertex_count > state.resident_vertex_count) || (state.host_index_count > state.resident_index_count) 
	       || (state.host_cluster_count > state.resident_cluster_count) || (state.host_lod_count > state.resident_lod_count);
}

export void renderer_geometry_copy_async(GPUCommandBuffer& cmd)
{
	auto vdiff = state.host_vertex_count - state.resident_vertex_count;
	if (vdiff)
	{
		gpu_mem_copy(cmd, state.host_vertex_pos + state.resident_vertex_count, state.vertex_pos + state.resident_vertex_count, vdiff * sizeof(geom_position_format));
		gpu_mem_copy(cmd, state.host_vertex_uv + state.resident_vertex_count, state.vertex_uv + state.resident_vertex_count, vdiff * sizeof(geom_uv_format));
		gpu_mem_copy(cmd, state.host_vertex_nor_tan + state.resident_vertex_count, state.vertex_nor_tan + state.resident_vertex_count, vdiff * sizeof(geom_nor_tan_format));
		state.resident_vertex_count = state.host_vertex_count;
	}

	auto idiff = state.host_index_count - state.resident_index_count;
	if(idiff)
	{
		gpu_mem_copy(cmd, state.host_indices + state.resident_index_count, state.indices + state.resident_index_count, idiff * sizeof(geom_index_format));
		state.resident_index_count = state.host_index_count;
	}

	auto cdiff = state.host_cluster_count - state.resident_cluster_count;
	if(cdiff)
	{
		gpu_mem_copy(cmd, state.host_clusters + state.resident_cluster_count, state.clusters + state.resident_cluster_count, cdiff * sizeof(geom_cluster_format));
		state.resident_cluster_count = state.host_cluster_count;
	}

	auto ldiff = state.host_lod_count - state.resident_lod_count;
	if(ldiff)
	{
		gpu_mem_copy(cmd, state.host_lods + state.resident_lod_count, state.lods + state.resident_lod_count, ldiff * sizeof(geom_lod_format));
		state.resident_lod_count = state.host_lod_count;
	}
}

export GPUDevicePointer renderer_geometry_vertex_pos_device_pointer()
{
	return gpu_host_to_device_pointer(state.vertex_pos);
}

export GPUDevicePointer renderer_geometry_vertex_uv_device_pointer()
{
	return gpu_host_to_device_pointer(state.vertex_uv);
}

export GPUDevicePointer renderer_geometry_vertex_nor_tan_device_pointer()
{
	return gpu_host_to_device_pointer(state.vertex_nor_tan);
}

export GPUPointer renderer_geometry_index_pointer()
{
	return state.indices;
}

export GPUDevicePointer renderer_geometry_cluster_device_pointer()
{
	return gpu_host_to_device_pointer(state.clusters);
}

export GPUDevicePointer renderer_geometry_lod_device_pointer()
{
	return gpu_host_to_device_pointer(state.lods);
}

}
