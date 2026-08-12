#pragma once 

#include <penumbra/array_proxy.hpp>
#include <penumbra/window.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/types.hpp>
#include <algorithm>
#include <string>

namespace penumbra
{

enum GPUMemoryHeap { GPU_MEMORY_HOST, GPU_MEMORY_PRIVATE, GPU_MEMORY_MAPPED, GPU_MEMORY_READBACK };
enum GPUBufferUsage { GPU_BUFFER_INVALID, GPU_BUFFER_STORAGE, GPU_BUFFER_UNIFORM, GPU_BUFFER_INDIRECT,
		      GPU_BUFFER_UPLOAD, GPU_BUFFER_VERTEX, GPU_BUFFER_INDEX };
enum GPUTextureType { GPU_TEXTURE_1D, GPU_TEXTURE_2D, GPU_TEXTURE_3D, GPU_TEXTURE_CUBE, GPU_TEXTURE_2D_ARRAY };
enum GPUFormat { GPU_FORMAT_UNDEFINED, GPU_FORMAT_R8_UNORM, GPU_FORMAT_RG8_UNORM, GPU_FORMAT_RGBA8_UNORM,
		GPU_FORMAT_RGBA8_SRGB, GPU_FORMAT_BGRA8_SRGB, GPU_FORMAT_RG16_SFLOAT, GPU_FORMAT_D16_UNORM, 
		GPU_FORMAT_D32_SFLOAT, GPU_FORMAT_R32_UINT, GPU_FORMAT_B10GR11_UFLOAT, GPU_FORMAT_RGBA16_SFLOAT, 
		GPU_FORMAT_BC4_UNORM, GPU_FORMAT_BC5_UNORM, GPU_FORMAT_BC6H_UFLOAT, GPU_FORMAT_BC7_UNORM, GPU_FORMAT_BC7_SRGB };
enum GPUTextureLayout { GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL, GPU_TEXTURE_LAYOUT_PRESENT };
enum GPUFilter { GPU_FILTER_NEAREST, GPU_FILTER_LINEAR };
enum GPUAddressMode { GPU_ADDRESS_MODE_REPEAT, GPU_ADDRESS_MODE_MIRRORED_REPEAT, GPU_ADDRESS_MODE_CLAMP_TO_EDGE, GPU_ADDRESS_MODE_CLAMP_TO_BORDER };
enum GPUCompareOp { GPU_COMPARE_OP_NONE, 
		    GPU_COMPARE_OP_NEVER, GPU_COMPARE_OP_LESS, 
		    GPU_COMPARE_OP_EQUAL, GPU_COMPARE_OP_LESS_EQUAL, 
		    GPU_COMPARE_OP_GREATER, GPU_COMPARE_OP_NOT_EQUAL, 
		    GPU_COMPARE_OP_GREATER_EQUAL, GPU_COMPARE_OP_ALWAYS };
enum GPUBlendOP { GPU_BLEND_OP_ADD, GPU_BLEND_OP_SUBTRACT, GPU_BLEND_OP_REV_SUBTRACT,
		  GPU_BLEND_OP_MIN, GPU_BLEND_OP_MAX };
enum GPUBlendFactor { GPU_BLEND_FACTOR_ZERO, GPU_BLEND_FACTOR_ONE, GPU_BLEND_FACTOR_SRC_COLOR,
		      GPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, GPU_BLEND_FACTOR_DST_COLOR,
		      GPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR, GPU_BLEND_FACTOR_SRC_ALPHA,
		      GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, GPU_BLEND_FACTOR_DST_ALPHA,
		      GPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA };
enum GPUTopology { GPU_TOPOLOGY_TRIANGLE_LIST, GPU_TOPOLOGY_TRIANGLE_STRIP, GPU_TOPOLOGY_LINE_LIST, GPU_TOPOLOGY_POINT_LIST };
enum GPUPolyMode { GPU_POLYMODE_FILL, GPU_POLYMODE_LINE, GPU_POLYMODE_POINT };
enum GPUCullMode { GPU_CULLMODE_NONE, GPU_CULLMODE_CCW, GPU_CULLMODE_CW, GPU_CULLMODE_ALL };
enum GPUQueue { GPU_QUEUE_INVALID, GPU_QUEUE_GRAPHICS, GPU_QUEUE_COMPUTE, GPU_QUEUE_TRANSFER };
enum GPUSemaphoreType { GPU_SEMAPHORE_TIMELINE, GPU_SEMAPHORE_BINARY };
enum GPULoadOP { GPU_LOAD_OP_LOAD, GPU_LOAD_OP_CLEAR, GPU_LOAD_OP_DONTCARE };
enum GPUStoreOP { GPU_STORE_OP_STORE, GPU_STORE_OP_DONTCARE };
enum GPUIndexType { GPU_INDEX_TYPE_U32, GPU_INDEX_TYPE_U16, GPU_INDEX_TYPE_U8 };
enum GPUPresentMode { GPU_PRESENT_MODE_FIFO, GPU_PRESENT_MODE_FIFO_RELAXED, GPU_PRESENT_MODE_IMMEDIATE };

enum GPUTextureUsage : u32
{
	GPU_TEXTURE_INVALID = 0,
	GPU_TEXTURE_SAMPLED = 0x1,
	GPU_TEXTURE_STORAGE = 0x2,
	GPU_TEXTURE_COLOR_ATTACHMENT = 0x4,
	GPU_TEXTURE_DEPTH_STENCIL_ATTACHMENT = 0x8,
	GPU_TEXTURE_READBACK = 0x10
};

enum GPUTextureDescriptorFlags : u32
{ 
	GPU_TEXTURE_DESCRIPTOR_RW = 0x1 
};

enum GPUDepthMode : u32 
{ 
	GPU_DEPTH_READ = 0x1, 
	GPU_DEPTH_WRITE = 0x2,
	GPU_DEPTH_CLAMP = 0x4	
};

enum GPUStage : u32
{
	GPU_STAGE_NONE = 0,
	GPU_STAGE_TRANSFER = 0x1,
	GPU_STAGE_COMMAND_PROCESSOR = 0x2,	
	GPU_STAGE_COMPUTE = 0x4,
	GPU_STAGE_RASTER_COLOR_OUTPUT = 0x8,
	GPU_STAGE_RASTER_DEPTH_OUTPUT = 0x10,
	GPU_STAGE_FRAGMENT_SHADER = 0x20,
	GPU_STAGE_VERTEX_SHADER = 0x40,
	GPU_STAGE_ALL = 0x80
};

enum GPUHazard : u32
{
	GPU_HAZARD_NONE = 0,
	GPU_HAZARD_INDIRECT_ARGS = 0x1,
	GPU_HAZARD_DEPTH_STENCIL = 0x2
};

constexpr u8 GPU_ALL_MIPS = ~0u;
constexpr u16 GPU_ALL_LAYERS = ~0u; 

using GPUDevicePointer = u64;
using GPUTexture = u32;
using GPUSampler = u32;
using GPUSemaphore = u32;

struct GPUPointer
{
	u64 handle;
	size_t offset{0};

	constexpr GPUPointer operator+(size_t off) const
	{
		return GPUPointer{handle, offset + off};
	}

	constexpr GPUPointer& operator+=(size_t off)
	{
		offset += off;
		return *this;
	}
};

struct GPUTextureDesc
{
	GPUTextureType type{GPU_TEXTURE_2D};
	uvec3 dim{0u, 0u, 1u};
	u32 mip_count{1};
	u32 layer_count{1};
	u32 sample_count{1};
	GPUFormat format{GPU_FORMAT_UNDEFINED};
	GPUTextureUsage usage{GPU_TEXTURE_INVALID};
};

struct GPUViewDesc
{
	GPUTextureType type{GPU_TEXTURE_2D};
	GPUFormat format{GPU_FORMAT_UNDEFINED};
	u8 base_mip{0};
	u8 mip_count{GPU_ALL_MIPS};
	u16 base_layer{0};
	u16 layer_count{GPU_ALL_LAYERS};
};

struct GPUTextureDescriptor
{
	u32 handle;
	u32 flags;
	GPUViewDesc desc;
};

struct GPUSamplerDesc
{
	GPUFilter mag_filter;
	GPUFilter min_filter;
	GPUFilter mip_filter;
	GPUAddressMode address_mode_u;
	GPUAddressMode address_mode_v;
	GPUAddressMode address_mode_w;
	GPUCompareOp compare_op{GPU_COMPARE_OP_NONE};
	float max_anisotropy{0.0f};
};

struct GPUPipeline
{
	u64 pso;
	u64 layout;
	u64 pdsl_handle;
	u32 cbuffer_size;
	u32 pconst_stage;
	u32 pconst_size;
	bool is_compute;
};

struct GPUDepthStencilDesc
{
	GPUDepthMode depth_mode{};
	GPUCompareOp depth_test{GPU_COMPARE_OP_ALWAYS};
};

struct GPUBlendDesc
{
	GPUBlendOP color_op{GPU_BLEND_OP_ADD};
	GPUBlendFactor src_color_factor{GPU_BLEND_FACTOR_ONE};
	GPUBlendFactor dst_color_factor{GPU_BLEND_FACTOR_ZERO};
	GPUBlendOP alpha_op{GPU_BLEND_OP_ADD};
	GPUBlendFactor src_alpha_factor{GPU_BLEND_FACTOR_ONE};
	GPUBlendFactor dst_alpha_factor{GPU_BLEND_FACTOR_ZERO};
	u8 color_write_mask = 0xf;
};

struct GPURasterDesc
{
	GPUTopology topology{GPU_TOPOLOGY_TRIANGLE_LIST};
	GPUPolyMode polymode{GPU_POLYMODE_FILL};
	GPUCullMode cull_mode{GPU_CULLMODE_NONE};
	array_proxy<GPUFormat> color_targets{};
	GPUFormat depth_format{GPU_FORMAT_UNDEFINED};
	GPUBlendDesc* blendstate{nullptr};
};

struct GPUCommandBuffer
{
	u32 thread{0};

	GPUPipeline* bound_pipe{nullptr};
	u64 handle{0};

	struct CB_Signal
	{
		GPUSemaphore object{0u};
		u64 value{0};
		GPUStage stage{};
	};

	CB_Signal wait_signal{};
	CB_Signal emit_signal{};	
};

struct GPURenderTarget
{
	GPUTexture texture{0u};
	GPUViewDesc view{};
	GPULoadOP load_op{GPU_LOAD_OP_DONTCARE};
	GPUStoreOP store_op{GPU_STORE_OP_STORE};
	float clear{0.0f};
};

struct GPURenderPassDesc
{
	uvec4 render_area{0u};
	array_proxy<GPURenderTarget> color_targets;
	GPURenderTarget depth_target{};
};

struct GPUIndirectCommand
{
	u32 vertex_count;
	u32 instance_count;
	u32 base_vertex;
	u32 base_instance;
};

struct GPUIndexedIndirectCommand
{
	u32 index_count;
	u32 instance_count;
	u32 index_offset;
	s32 vertex_offset;
	u32 instance_id;
};

struct GPUProperties
{
	std::string device_name;
};

bool gpu_init();
void gpu_shutdown();

GPUPointer gpu_allocate_memory(size_t size, GPUMemoryHeap heap = GPU_MEMORY_PRIVATE, GPUBufferUsage usage = GPU_BUFFER_STORAGE);
void gpu_free_memory(GPUPointer& ptr);
GPUDevicePointer gpu_host_to_device_pointer(const GPUPointer& ptr); 
u8* gpu_map_memory(const GPUPointer& ptr);

GPUTexture gpu_create_texture(const GPUTextureDesc& desc);
void gpu_destroy_texture(GPUTexture tex);
GPUTextureDescriptor gpu_texture_view_descriptor(GPUTexture tex, const GPUViewDesc& desc);
GPUTextureDescriptor gpu_rwtexture_view_descriptor(GPUTexture tex, const GPUViewDesc& desc);
GPUSampler gpu_create_sampler(const GPUSamplerDesc& desc);
void gpu_free_descriptor(GPUTextureDescriptor descriptor);

struct ShaderIR;
GPUPipeline gpu_create_compute_pipeline(const ShaderIR& shader);
GPUPipeline gpu_create_graphics_pipeline(const ShaderIR& shader, const GPURasterDesc& raster);
void gpu_destroy_pipeline(GPUPipeline& pipe);

GPUCommandBuffer gpu_record_commands(GPUQueue queue);
u64 gpu_submit(GPUQueue queue, GPUCommandBuffer& cmd);
bool gpu_wait_queue(GPUQueue queue, u64 timeline);
void gpu_wait_idle();

GPUSemaphore gpu_create_semaphore(u64 initial_value, GPUSemaphoreType type = GPU_SEMAPHORE_TIMELINE);
void gpu_destroy_semaphore(GPUSemaphore sem);
u64 gpu_semaphore_read_counter(GPUSemaphore semaphore);

void gpu_mem_copy(const GPUCommandBuffer& cmd, const GPUPointer& src, const GPUPointer& dst, size_t size);
void gpu_mem_clear(const GPUCommandBuffer& cmd, const GPUPointer& dst, size_t size);
void gpu_copy_to_texture(const GPUCommandBuffer& cmd, const GPUPointer& src, GPUTexture dst, u32 mips = 1, u32 layers = 1);
void gpu_copy_from_texture(const GPUCommandBuffer& cmd, GPUTexture src, const GPUPointer& dst);

void gpu_barrier(const GPUCommandBuffer& cmd, GPUStage src, GPUStage dst, GPUHazard hazards = GPU_HAZARD_NONE);
void gpu_texture_layout_transition(const GPUCommandBuffer& cmd, GPUTexture tex, GPUStage src_stage, GPUStage dst_stage, GPUTextureLayout src_layout, GPUTextureLayout dst_layout, GPUQueue src_queue = GPU_QUEUE_INVALID, GPUQueue dst_queue = GPU_QUEUE_INVALID);
void gpu_wait_signal(GPUCommandBuffer& cmd, GPUStage dst_stage, GPUSemaphore sem, u64 timeline);
void gpu_emit_signal(GPUCommandBuffer& cmd, GPUStage src_stage, GPUSemaphore sem, u64 timeline);

void gpu_set_pipeline(GPUCommandBuffer& cmd, GPUPipeline& pipe);
void gpu_set_depth_stencil_state(const GPUCommandBuffer& cmd, const GPUDepthStencilDesc& state); 
void gpu_write_cbuffer_descriptor(const GPUCommandBuffer& cmd, const GPUPointer& cbuffer);

void gpu_dispatch(const GPUCommandBuffer& cmd, void* data, uvec3 dim);
void gpu_dispatch_indirect(const GPUCommandBuffer& cmd, void* data, const GPUPointer& dim);

void gpu_begin_renderpass(const GPUCommandBuffer& cmd, const GPURenderPassDesc& rp);
void gpu_end_renderpass(const GPUCommandBuffer& cmd);

void gpu_set_scissor(const GPUCommandBuffer& cmd, uvec4 scissor);
void gpu_set_cullmode(const GPUCommandBuffer& cmd, GPUCullMode mode);
void gpu_bind_index_buffer(const GPUCommandBuffer& cmd, const GPUPointer& buffer, GPUIndexType type);

void gpu_draw(const GPUCommandBuffer& cmd, void* data, u32 vertex_count, u32 instance_count, u32 base_vertex, u32 base_instance);
void gpu_draw_indexed(const GPUCommandBuffer& cmd, void* data, u32 index_count, u32 instance_count, u32 base_index, u32 base_vertex, u32 base_instance);
void gpu_draw_indirect(const GPUCommandBuffer& cmd, void* data, const GPUPointer& commands, u32 draw_count);
void gpu_draw_indexed_indirect_count(const GPUCommandBuffer& cmd, void* data, const GPUPointer& commands, const GPUPointer& draw_count, u32 max_draw_count);

void gpu_swapchain_init(window_t wnd);
GPUTexture gpu_swapchain_acquire_next(GPUSemaphore sem);
void gpu_swapchain_present(GPUQueue queue, GPUSemaphore sem);
bool gpu_swapchain_set_present_mode(GPUPresentMode mode);
const GPUProperties& gpu_get_properties();

constexpr GPUTextureUsage operator|(GPUTextureUsage lhs, GPUTextureUsage rhs)
{
	return static_cast<GPUTextureUsage>(static_cast<u32>(lhs) | static_cast<u32>(rhs));
}

constexpr GPUDepthMode operator|(GPUDepthMode lhs, GPUDepthMode rhs)
{
	return static_cast<GPUDepthMode>(static_cast<u32>(lhs) | static_cast<u32>(rhs));
}

constexpr GPUStage operator|(GPUStage lhs, GPUStage rhs)
{
	return static_cast<GPUStage>(static_cast<u32>(lhs) | static_cast<u32>(rhs));
}

constexpr GPUHazard operator|(GPUHazard lhs, GPUHazard rhs)
{
	return static_cast<GPUHazard>(static_cast<u32>(lhs) | static_cast<u32>(rhs));
}

constexpr u32 gpu_format_blockdim(GPUFormat fmt)
{
	switch(fmt)
	{
	case GPU_FORMAT_BC5_UNORM:
	case GPU_FORMAT_BC6H_UFLOAT:
	case GPU_FORMAT_BC7_UNORM:
	case GPU_FORMAT_BC7_SRGB:
		return 4u;
	default:
		return 1u;
	}
}

constexpr u32 gpu_format_blocksize(GPUFormat fmt)
{
	switch(fmt)
	{
	case GPU_FORMAT_BC5_UNORM:
	case GPU_FORMAT_BC6H_UFLOAT:
	case GPU_FORMAT_BC7_UNORM:
	case GPU_FORMAT_BC7_SRGB:
		return 16u;
	case GPU_FORMAT_RGBA16_SFLOAT:
		return 8u;
	case GPU_FORMAT_R32_UINT:
	case GPU_FORMAT_RG16_SFLOAT:
	case GPU_FORMAT_RGBA8_SRGB:
	case GPU_FORMAT_RGBA8_UNORM:
	case GPU_FORMAT_BGRA8_SRGB:
	case GPU_FORMAT_B10GR11_UFLOAT:
		return 4u;
	case GPU_FORMAT_RG8_UNORM:
		return 2u;
	default:
		return 1u;
	}
}

constexpr u32 gpu_format_size(GPUFormat fmt, u32 w, u32 h, u32 d)
{
	u32 bdim = gpu_format_blockdim(fmt);
	w = std::max(w / bdim, 1u);
	h = std::max(h / bdim, 1u);
	d = std::max(d / bdim, 1u);
	
	return w * h * d * gpu_format_blocksize(fmt);
}	

}
