#pragma once

#include <penumbra/gpu.hpp>
#include <volk.h>

namespace penumbra
{

constexpr VkBufferUsageFlags2 decode_buffer_usage(GPUBufferUsage usage)
{
	switch(usage)
	{
	case GPU_BUFFER_STORAGE:
		return VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	case GPU_BUFFER_UNIFORM:
		return VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT;
	case GPU_BUFFER_INDIRECT:
		return VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	case GPU_BUFFER_UPLOAD:
		return VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
	case GPU_BUFFER_VERTEX:
		return VK_BUFFER_USAGE_2_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
	case GPU_BUFFER_INDEX:
		return VK_BUFFER_USAGE_2_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT;
	default:
		std::unreachable();
	}
}

constexpr VkMemoryPropertyFlags decode_memory_heap(GPUMemoryHeap heap)
{
	switch(heap)
	{
	case GPU_MEMORY_HOST:
		return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	case GPU_MEMORY_PRIVATE:
		return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	case GPU_MEMORY_MAPPED:
		return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	case GPU_MEMORY_READBACK:
		return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	default:
		std::unreachable();
	}
}

constexpr VkImageType image_type_to_vk(GPUTextureType type)
{
	switch(type)
	{
	case GPU_TEXTURE_1D:
		return VK_IMAGE_TYPE_1D;
	case GPU_TEXTURE_2D:
	case GPU_TEXTURE_CUBE:
	case GPU_TEXTURE_2D_ARRAY:
		return VK_IMAGE_TYPE_2D;
	case GPU_TEXTURE_3D:
		return VK_IMAGE_TYPE_3D;
	default:
		std::unreachable();
	}
}

constexpr VkImageViewType image_view_type_to_vk(GPUTextureType type)
{
	switch(type)
	{
	case GPU_TEXTURE_1D:
		return VK_IMAGE_VIEW_TYPE_1D;
	case GPU_TEXTURE_2D:
		return VK_IMAGE_VIEW_TYPE_2D;
	case GPU_TEXTURE_3D:
		return VK_IMAGE_VIEW_TYPE_3D;
	case GPU_TEXTURE_CUBE:
		return VK_IMAGE_VIEW_TYPE_CUBE;
	case GPU_TEXTURE_2D_ARRAY:
		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	default:
		std::unreachable();
	}
}

constexpr VkFormat format_to_vk(GPUFormat fmt)
{
	switch(fmt)
	{
	case GPU_FORMAT_UNDEFINED:
		return VK_FORMAT_UNDEFINED;
	case GPU_FORMAT_R8_UNORM:
		return VK_FORMAT_R8_UNORM;
	case GPU_FORMAT_RG8_UNORM:
		return VK_FORMAT_R8G8_UNORM;
	case GPU_FORMAT_RGBA8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case GPU_FORMAT_RGBA8_SRGB:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case GPU_FORMAT_BGRA8_SRGB:
		return VK_FORMAT_B8G8R8A8_SRGB;
	case GPU_FORMAT_RG16_SFLOAT:
		return VK_FORMAT_R16G16_SFLOAT;
	case GPU_FORMAT_D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case GPU_FORMAT_D32_SFLOAT:
		return VK_FORMAT_D32_SFLOAT;
	case GPU_FORMAT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case GPU_FORMAT_B10GR11_UFLOAT:
		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	case GPU_FORMAT_RGBA16_SFLOAT:
		return VK_FORMAT_R16G16B16A16_SFLOAT;
	case GPU_FORMAT_BC4_UNORM:
		return VK_FORMAT_BC4_UNORM_BLOCK;
	case GPU_FORMAT_BC5_UNORM:
		return VK_FORMAT_BC5_UNORM_BLOCK;
	case GPU_FORMAT_BC6H_UFLOAT:
		return VK_FORMAT_BC6H_UFLOAT_BLOCK;
	case GPU_FORMAT_BC7_UNORM:
		return VK_FORMAT_BC7_UNORM_BLOCK;
	case GPU_FORMAT_BC7_SRGB:
		return VK_FORMAT_BC7_SRGB_BLOCK;
	default:
		std::unreachable();
	}
}

constexpr VkImageAspectFlagBits format_to_vk_aspect(GPUFormat fmt)
{
	switch(fmt)
	{
	case GPU_FORMAT_D16_UNORM:
	case GPU_FORMAT_D32_SFLOAT:
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	default:
		return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

constexpr VkSampleCountFlagBits sample_count_to_vk(u32 sample_count)
{
	switch(sample_count)
	{
	case 1:
		return VK_SAMPLE_COUNT_1_BIT;
	case 2:
		return VK_SAMPLE_COUNT_2_BIT;
	case 4:
		return VK_SAMPLE_COUNT_4_BIT;
	case 8:
		return VK_SAMPLE_COUNT_8_BIT;
	default:
		std::unreachable();
	}
}

constexpr VkImageUsageFlags image_usage_to_vk(GPUTextureUsage usage)
{
	VkImageUsageFlags res{VK_IMAGE_USAGE_TRANSFER_DST_BIT};

	if(usage & GPU_TEXTURE_SAMPLED)
		res |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if(usage & GPU_TEXTURE_STORAGE)
		res |= VK_IMAGE_USAGE_STORAGE_BIT;
	if(usage & GPU_TEXTURE_COLOR_ATTACHMENT)
		res |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if(usage & GPU_TEXTURE_DEPTH_STENCIL_ATTACHMENT)
		res |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if(usage & GPU_TEXTURE_READBACK)
		res |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	return res;
}

constexpr VkFilter filter_to_vk(GPUFilter filter)
{
	switch(filter)
	{
	case GPU_FILTER_NEAREST:
		return VK_FILTER_NEAREST;
	case GPU_FILTER_LINEAR:
		return VK_FILTER_LINEAR;
	default:
		std::unreachable();
	}
}

constexpr VkSamplerMipmapMode filter_to_mipmap_vk(GPUFilter filter)
{
	switch(filter)
	{
	case GPU_FILTER_NEAREST:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;
	case GPU_FILTER_LINEAR:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	default:
		std::unreachable();
	}
}

constexpr VkSamplerAddressMode address_mode_to_vk(GPUAddressMode mode)
{
	switch(mode)
	{
	case GPU_ADDRESS_MODE_REPEAT:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case GPU_ADDRESS_MODE_MIRRORED_REPEAT:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	case GPU_ADDRESS_MODE_CLAMP_TO_EDGE:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case GPU_ADDRESS_MODE_CLAMP_TO_BORDER:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	default:
		std::unreachable();
	}
}

constexpr VkPipelineStageFlags2 gpu_stage_to_vk(GPUStage stage)
{
	VkPipelineStageFlags2 res{0};

	if(stage & GPU_STAGE_TRANSFER)
		res |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	if(stage & GPU_STAGE_COMMAND_PROCESSOR)
		res |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	if(stage & GPU_STAGE_COMPUTE)
		res |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	if(stage & GPU_STAGE_RASTER_COLOR_OUTPUT)
		res |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	if(stage & GPU_STAGE_RASTER_DEPTH_OUTPUT)
		res |= (VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
	if(stage & GPU_STAGE_FRAGMENT_SHADER)
		res |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	if(stage & GPU_STAGE_VERTEX_SHADER)
		res |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	if(stage & GPU_STAGE_ALL)
		res |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	return res;
}

constexpr VkShaderStageFlags shader_stage_to_vk_flags(GPUStage stage)
{
	VkShaderStageFlags res{};

	if(stage & GPU_STAGE_VERTEX_SHADER)
		res |= VK_SHADER_STAGE_VERTEX_BIT;
	if(stage & GPU_STAGE_FRAGMENT_SHADER)
		res |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if(stage & GPU_STAGE_COMPUTE)
		res |= VK_SHADER_STAGE_COMPUTE_BIT;

	return res;
}

constexpr VkShaderStageFlagBits shader_stage_to_vk(GPUStage stage)
{
	switch(stage)
	{
	case GPU_STAGE_VERTEX_SHADER:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case GPU_STAGE_FRAGMENT_SHADER:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case GPU_STAGE_COMPUTE:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	default:
		std::unreachable();
	}
}

constexpr VkPrimitiveTopology raster_topology_to_vk(GPUTopology topo)
{
	switch(topo)
	{
	case GPU_TOPOLOGY_TRIANGLE_LIST:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	case GPU_TOPOLOGY_TRIANGLE_STRIP:
		return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	case GPU_TOPOLOGY_LINE_LIST:
		return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	case GPU_TOPOLOGY_POINT_LIST:
		return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	default:
		std::unreachable();
	}
}

constexpr VkPolygonMode raster_polymode_to_vk(GPUPolyMode mode)
{
	switch(mode)
	{
	case GPU_POLYMODE_FILL:
		return VK_POLYGON_MODE_FILL;
	case GPU_POLYMODE_LINE:
		return VK_POLYGON_MODE_LINE;
	case GPU_POLYMODE_POINT:
		return VK_POLYGON_MODE_POINT;
	default:
		std::unreachable();
	}
}

constexpr VkCullModeFlags raster_cullmode_to_vk(GPUCullMode mode)
{
	switch(mode)
	{
	case GPU_CULLMODE_NONE:
		return VK_CULL_MODE_NONE;
	case GPU_CULLMODE_CCW:
		return VK_CULL_MODE_FRONT_BIT;
	case GPU_CULLMODE_CW:
		return VK_CULL_MODE_BACK_BIT;
	case GPU_CULLMODE_ALL:
		return VK_CULL_MODE_FRONT_AND_BACK;
	default:
		std::unreachable();
	}
}

constexpr VkBlendFactor blend_factor_to_vk(GPUBlendFactor factor)
{
	switch(factor)
	{
	case GPU_BLEND_FACTOR_ZERO:
		return VK_BLEND_FACTOR_ZERO;
	case GPU_BLEND_FACTOR_ONE:
		return VK_BLEND_FACTOR_ONE;
	case GPU_BLEND_FACTOR_SRC_COLOR:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case GPU_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case GPU_BLEND_FACTOR_DST_COLOR:
		return VK_BLEND_FACTOR_DST_COLOR;
	case GPU_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case GPU_BLEND_FACTOR_SRC_ALPHA:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case GPU_BLEND_FACTOR_DST_ALPHA:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case GPU_BLEND_FACTOR_ONE_MINUS_DST_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	default:
		std::unreachable();
	}
}

constexpr VkBlendOp blend_op_to_vk(GPUBlendOP op)
{
	switch(op)
	{
	case GPU_BLEND_OP_ADD:
		return VK_BLEND_OP_ADD;
	case GPU_BLEND_OP_SUBTRACT:
		return VK_BLEND_OP_SUBTRACT;
	case GPU_BLEND_OP_REV_SUBTRACT:
		return VK_BLEND_OP_REVERSE_SUBTRACT;
	case GPU_BLEND_OP_MIN:
		return VK_BLEND_OP_MIN;
	case GPU_BLEND_OP_MAX:
		return VK_BLEND_OP_MAX;
	default:
		std::unreachable();
	}
}

constexpr VkImageLayout texlayout_to_vk(GPUTextureLayout layout)
{
	switch(layout)
	{
	case GPU_TEXTURE_LAYOUT_UNDEFINED:
		return VK_IMAGE_LAYOUT_UNDEFINED;
	case GPU_TEXTURE_LAYOUT_GENERAL:
		return VK_IMAGE_LAYOUT_GENERAL;
	case GPU_TEXTURE_LAYOUT_PRESENT:
		return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	default:
		std::unreachable();
	}
}

constexpr VkCompareOp compare_op_to_vk(GPUCompareOp op)
{
	switch(op)
	{
	case GPU_COMPARE_OP_NEVER:
		return VK_COMPARE_OP_NEVER;
	case GPU_COMPARE_OP_LESS:
		return VK_COMPARE_OP_LESS;
	case GPU_COMPARE_OP_EQUAL:
		return VK_COMPARE_OP_EQUAL;
	case GPU_COMPARE_OP_LESS_EQUAL:
		return VK_COMPARE_OP_LESS_OR_EQUAL;
	case GPU_COMPARE_OP_GREATER:
		return VK_COMPARE_OP_GREATER;
	case GPU_COMPARE_OP_NOT_EQUAL:
		return VK_COMPARE_OP_NOT_EQUAL;
	case GPU_COMPARE_OP_GREATER_EQUAL:
		return VK_COMPARE_OP_GREATER_OR_EQUAL;
	case GPU_COMPARE_OP_ALWAYS:
	case GPU_COMPARE_OP_NONE:
		return VK_COMPARE_OP_ALWAYS;
	default:
		std::unreachable();
	}
}


constexpr VkAttachmentLoadOp load_op_to_vk(GPULoadOP op)
{
	switch(op)
	{
	case GPU_LOAD_OP_LOAD:
		return VK_ATTACHMENT_LOAD_OP_LOAD;
	case GPU_LOAD_OP_CLEAR:
		return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case GPU_LOAD_OP_DONTCARE:
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	default:
		std::unreachable();
	}
}

constexpr VkAttachmentStoreOp store_op_to_vk(GPUStoreOP op)
{
	switch(op)
	{
	case GPU_STORE_OP_STORE:
		return VK_ATTACHMENT_STORE_OP_STORE;
	case GPU_STORE_OP_DONTCARE:
		return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	default:
		std::unreachable();
	}
}

constexpr VkIndexType index_type_to_vk(GPUIndexType type)
{
	switch(type)
	{
	case GPU_INDEX_TYPE_U16:
		return VK_INDEX_TYPE_UINT16;
	case GPU_INDEX_TYPE_U32:
		return VK_INDEX_TYPE_UINT32;
	case GPU_INDEX_TYPE_U8:
		return VK_INDEX_TYPE_UINT8;
	default:
		std::unreachable();
	}
}

constexpr VkPresentModeKHR present_mode_to_vk(GPUPresentMode mode)
{
	switch(mode)
	{
	case GPU_PRESENT_MODE_FIFO:
		return VK_PRESENT_MODE_FIFO_KHR;
	case GPU_PRESENT_MODE_FIFO_RELAXED:
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	case GPU_PRESENT_MODE_IMMEDIATE:
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	default:
		std::unreachable();
	}
}

}
