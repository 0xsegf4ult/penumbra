#include <renderer/resource.hpp>
#include <penumbra/types.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/log.hpp>
#include <penumbra/panic.hpp>

#include <format>
#include <vector>

namespace penumbra
{

struct streambuffer_chunk
{
	constexpr static size_t size = 32 * 1024 * 1024;
	GPUPointer data;
	u64 syncval{0};
	u32 head{0u};
};

struct buffer_write_request
{
	GPUPointer src;
	GPUPointer dst;
	size_t size;
};

struct texture_write_request
{
	GPUPointer data;
	GPUTexture texture;
	u32 num_mips;
	u32 num_layers;
};

struct renderer_resource_state
{
	GPUSemaphore transfer_semaphore;
	u64 transfer_sync{0u};

	std::vector<streambuffer_chunk> streambuffer;
	std::vector<buffer_write_request> bufwrites;
	std::vector<texture_write_request> texwrites;

	GPUPointer geometry_vertex_pos;
	GPUPointer geometry_vertex_uv;
	GPUPointer geometry_vertex_nor_tan;
	GPUPointer geometry_skinned_vertex;
	GPUPointer geometry_index;
	GPUPointer geometry_cluster;
	GPUPointer geometry_lod;

	u32 geom_vertex_size{0u};
	u32 geom_skinned_vert_size{0u};
	u32 geom_index_size{0u};
	u32 geom_cluster_size{0u};
	u32 geom_lod_size{0u};

	u32 geom_vertex_capacity{10000000u};
	u32 geom_skinned_vert_capacity{100000u};
	u32 geom_index_capacity{50000000u};
        u32 geom_cluster_capacity{131072u};
	u32 geom_lod_capacity{65536u};

	GPUPointer material_buffer;
	u32 material_buffer_capacity{65536u};

	GPUPointer bone_buffer;
	u32 bone_size{0u};
	u32 bone_capacity{16384u};
};

static renderer_resource_state* state = nullptr;

void renderer_resource_state_init()
{
	state = new renderer_resource_state();
	state->transfer_semaphore = gpu_create_semaphore(0);

	state->streambuffer.push_back
	({
		gpu_allocate_memory(streambuffer_chunk::size, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD),
		0u, 0u
	});
	
	log::info("renderer: streambuffer size {} KB", state->streambuffer.size() * streambuffer_chunk::size / 1024);

	state->geometry_vertex_pos = gpu_allocate_memory(state->geom_vertex_capacity * sizeof(geom_position_format));
	state->geometry_vertex_uv = gpu_allocate_memory(state->geom_vertex_capacity * sizeof(geom_uv_format));
	state->geometry_vertex_nor_tan = gpu_allocate_memory(state->geom_vertex_capacity * sizeof(geom_nor_tan_format));
	
	state->geometry_skinned_vertex = gpu_allocate_memory(state->geom_skinned_vert_capacity * sizeof(geom_skinned_format));
	state->geometry_index = gpu_allocate_memory(state->geom_index_capacity * sizeof(geom_index_format), GPU_MEMORY_PRIVATE, GPU_BUFFER_INDEX);
	state->geometry_cluster = gpu_allocate_memory(state->geom_cluster_capacity * sizeof(geom_cluster_format));
	state->geometry_lod = gpu_allocate_memory(state->geom_lod_capacity * sizeof(geom_lod_format));

	state->material_buffer = gpu_allocate_memory(state->material_buffer_capacity * sizeof(render_material_data), GPU_MEMORY_MAPPED);
	renderer_write_material(0, render_material_data{});

	state->bone_buffer = gpu_allocate_memory(state->bone_capacity * sizeof(mat4), GPU_MEMORY_MAPPED);
}

void renderer_resource_state_shutdown()
{
	gpu_free_memory(state->bone_buffer);

	gpu_free_memory(state->material_buffer);

	gpu_free_memory(state->geometry_lod);
	gpu_free_memory(state->geometry_cluster);
	gpu_free_memory(state->geometry_index);
	gpu_free_memory(state->geometry_skinned_vertex);
	gpu_free_memory(state->geometry_vertex_nor_tan);
	gpu_free_memory(state->geometry_vertex_uv);
	gpu_free_memory(state->geometry_vertex_pos);

	for(auto& chunk : state->streambuffer)
		gpu_free_memory(chunk.data);

	gpu_destroy_semaphore(state->transfer_semaphore);
	delete state;
}

u64 renderer_resource_transfer_syncval()
{
	return state->transfer_sync;
}

void renderer_resource_copy_async()
{
	auto transfer_time = gpu_semaphore_read_counter(state->transfer_semaphore);
	for(auto& chunk : state->streambuffer)
	{
		if(chunk.syncval <= transfer_time)
			chunk.head = 0;
	}

	if(state->texwrites.empty() && state->bufwrites.empty())
		return;

	auto cmd = gpu_record_commands(GPU_QUEUE_TRANSFER);

	for(auto& write : state->bufwrites)
		gpu_mem_copy(cmd, write.src, write.dst, write.size);

	state->bufwrites.clear();

	for(auto& write : state->texwrites)
	{
		gpu_texture_layout_transition(cmd, write.texture, GPU_STAGE_NONE, GPU_STAGE_TRANSFER, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
		gpu_copy_to_texture(cmd, write.data, write.texture, write.num_mips, write.num_layers);
	}
	state->texwrites.clear();

	state->transfer_sync++;
	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_ALL);
	gpu_emit_signal(cmd, GPU_STAGE_ALL, state->transfer_semaphore, state->transfer_sync);
	gpu_submit(GPU_QUEUE_TRANSFER, cmd);
}

static streambuffer_chunk& stream_buffer_acquire(size_t size)
{
	if(size > streambuffer_chunk::size)
	{
		auto msg = std::format("stream_buffer_acquire: data size {} larger than chunk size {}", size, streambuffer_chunk::size);
		panic(msg.c_str());
	}

	for(auto& chunk: state->streambuffer)
	{
		if(chunk.head + size <= streambuffer_chunk::size)
			return chunk;
	}


	state->streambuffer.push_back
	({
		gpu_allocate_memory(streambuffer_chunk::size, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD),
		0u, 0u
	});

	log::info("renderer: streambuffer size {} KB", state->streambuffer.size() * streambuffer_chunk::size / 1024);

	return state->streambuffer.back();
}

u32 renderer_geometry_reserve_vertices(u32 count)
{
	auto offset = state->geom_vertex_size;
	if(offset + count >= state->geom_vertex_capacity)
	{
		log::warn("renderer: out of vertex memory");
		return offset;
	}
	
	state->geom_vertex_size += count;
	return offset;
}

u32 renderer_geometry_write_vertices(const geom_position_format* pos_data, const geom_uv_format* uv_data, const geom_nor_tan_format* nrm_data, u32 count)
{
	auto offset = state->geom_vertex_size;
	if(offset + count >= state->geom_vertex_capacity)
	{
		log::warn("renderer: out of vertex memory!");
		return offset;
	}

	auto syncval = state->transfer_sync + 1;

	{
		auto size = count * sizeof(geom_position_format);
		auto& stream_block = stream_buffer_acquire(size);
		memcpy(gpu_map_memory(stream_block.data + stream_block.head), pos_data, size);
		state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_vertex_pos + (offset * sizeof(geom_position_format)), size});
		stream_block.head += size;
		stream_block.syncval = syncval;
	}

	{
		auto size = count * sizeof(geom_uv_format);
		auto& stream_block = stream_buffer_acquire(size);
		memcpy(gpu_map_memory(stream_block.data + stream_block.head), uv_data, size);
		state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_vertex_uv + (offset * sizeof(geom_uv_format)), size});
		stream_block.head += size;
		stream_block.syncval = syncval;
	}

	{
		auto size = count * sizeof(geom_nor_tan_format);
		auto& stream_block = stream_buffer_acquire(size);
		memcpy(gpu_map_memory(stream_block.data + stream_block.head), nrm_data, size);
		state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_vertex_nor_tan + (offset * sizeof(geom_nor_tan_format)), size});
		stream_block.head += size;
		stream_block.syncval = syncval;
	}

	state->geom_vertex_size += count;
	return offset;
}

u32 renderer_geometry_write_skinned(const geom_skinned_format* data, u32 count)
{
	auto offset = state->geom_skinned_vert_size;
	if(offset + count >= state->geom_skinned_vert_capacity)
	{	
		log::warn("renderer: out of skinned vertex memory!");
		return offset;
	}

	auto size = count * sizeof(geom_skinned_format);
	auto& stream_block = stream_buffer_acquire(size);
	memcpy(gpu_map_memory(stream_block.data + stream_block.head), data, size);
	state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_skinned_vertex + (offset * sizeof(geom_skinned_format)), size});
	stream_block.head += size;
	stream_block.syncval = state->transfer_sync + 1;
	state->geom_skinned_vert_size += count;
	return offset;
}

u32 renderer_geometry_write_indices(const geom_index_format* data, u32 count)
{
	auto offset = state->geom_index_size;
	if(offset + count >= state->geom_index_capacity)
	{
		log::warn("renderer: out of index memory!");
		return offset;
	}

	auto size = count * sizeof(geom_index_format);
	auto& stream_block = stream_buffer_acquire(size);
	memcpy(gpu_map_memory(stream_block.data + stream_block.head), data, size);
	state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_index + (offset * sizeof(geom_index_format)), size});
	stream_block.head += size;
	stream_block.syncval = state->transfer_sync + 1;
	state->geom_index_size += count;
	return offset;
}

u32 renderer_geometry_write_clusters(const geom_cluster_format* data, u32 count)
{
	auto offset = state->geom_cluster_size;
	if(offset + count >= state->geom_cluster_capacity)
	{
		log::warn("renderer: out of cluster memory!");
		return offset;
	}

	auto size = count * sizeof(geom_cluster_format);
	auto& stream_block = stream_buffer_acquire(size);
	memcpy(gpu_map_memory(stream_block.data + stream_block.head), data, size);
	state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_cluster + (offset * sizeof(geom_cluster_format)), size});
	stream_block.head += size;
	stream_block.syncval = state->transfer_sync + 1;
	state->geom_cluster_size += count;
	return offset;
}

u32 renderer_geometry_write_lods(const geom_lod_format* data, u32 count)
{
	auto offset = state->geom_lod_size;
	if(offset + count >= state->geom_lod_capacity)
	{
		log::warn("renderer: out of lod memory!");
		return offset;
	}

	auto size = count * sizeof(geom_lod_format);
	auto& stream_block = stream_buffer_acquire(size);
	memcpy(gpu_map_memory(stream_block.data + stream_block.head), data, size);
	state->bufwrites.push_back({stream_block.data + stream_block.head, state->geometry_lod + (offset * sizeof(geom_lod_format)), size});
	stream_block.head += size;
	stream_block.syncval = state->transfer_sync + 1;
	state->geom_lod_size += count;
	return offset;
}

renderer_geometry_storage renderer_geometry_get_storage()
{
	return
	{
		state->geometry_vertex_pos,
		state->geometry_vertex_uv,
		state->geometry_vertex_nor_tan,
		state->geometry_skinned_vertex,
		state->geometry_index,
		state->geometry_cluster,
		state->geometry_lod
	};
}

void renderer_write_texture(GPUTexture texture, std::span<const u8> data, u32 num_mips, u32 num_layers)
{
	auto& stream_block = stream_buffer_acquire(data.size());
	memcpy(gpu_map_memory(stream_block.data + stream_block.head), data.data(), data.size());
	state->texwrites.push_back({stream_block.data + stream_block.head, texture, num_mips, num_layers});
	stream_block.head += data.size();
	stream_block.syncval = state->transfer_sync + 1;
}

void renderer_write_material(u32 offset, const render_material_data& data)
{
	if(offset >= state->material_buffer_capacity)
	{
		log::warn("renderer_write_material: index [{}] out of range", offset);
		return;
	}

	memcpy(gpu_map_memory(state->material_buffer + offset * sizeof(render_material_data)), &data, sizeof(render_material_data));
}

void renderer_write_bones(u32 offset, const mat4* data, u16 count)
{
	if(offset >= state->bone_capacity)
	{
		log::warn("renderer_write_bones: index [{}] out of range", offset);
		return;
	}

	memcpy(gpu_map_memory(state->bone_buffer + offset * sizeof(mat4)), data, sizeof(mat4) * count);
}

renderer_skinned_geometry_instance renderer_geometry_instantiate_skin(u32 vertex_offset, u32 vertex_count, u16 bone_count)
{
	auto new_vtx_offset = renderer_geometry_reserve_vertices(vertex_count);
	auto bone_offset = state->bone_size;
	if(bone_offset + bone_count >= state->bone_capacity)
	{
		log::error("renderer: out of bone memory!");
		return {0, 0, 0, 0};
	}

	state->bone_size += bone_count;

	return 
	{
		bone_offset,
		new_vtx_offset,
		vertex_offset,
		vertex_count
	};
}

GPUPointer renderer_material_get_storage()
{
	return state->material_buffer;
}

GPUPointer renderer_bones_get_storage()
{
	return state->bone_buffer;
}

}
