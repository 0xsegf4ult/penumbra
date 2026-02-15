module;

#include <volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_error.h>
#include <cassert>
#include <tracy/Tracy.hpp>

module penumbra.gpu;

import :shader;

import penumbra.core;
import std;

using std::uint32_t, std::uint64_t, std::size_t;

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
	case GPU_FORMAT_D16_UNORM:
		return VK_FORMAT_D16_UNORM;
	case GPU_FORMAT_D32_SFLOAT:
		return VK_FORMAT_D32_SFLOAT;
	case GPU_FORMAT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case GPU_FORMAT_B10GR11_UFLOAT:
		return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
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

constexpr VkSampleCountFlagBits sample_count_to_vk(uint32_t sample_count)
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
	default:
		std::unreachable();
	}
}

constexpr VkPipelineStageFlags2 gpu_stage_to_vk(GPUStage stage)
{
	VkPipelineStageFlags2 res{0};

	if(stage & GPU_STAGE_TRANSFER)
		res |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	if(stage & GPU_STAGE_COMPUTE)
		res |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	if(stage & GPU_STAGE_RASTER_OUTPUT)
		res |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	if(stage & GPU_STAGE_FRAGMENT_SHADER)
		res |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	if(stage & GPU_STAGE_VERTEX_SHADER)
		res |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	if(stage & GPU_STAGE_ALL)
		res |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;

	return res;
}

constexpr VkShaderStageFlags shader_stage_to_vk_flags(ShaderPipelineStage stage)
{
	VkShaderStageFlags res{};

	if(stage & SHADER_STAGE_VERTEX)
		res |= VK_SHADER_STAGE_VERTEX_BIT;
	if(stage & SHADER_STAGE_FRAGMENT)
		res |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if(stage & SHADER_STAGE_COMPUTE)
		res |= VK_SHADER_STAGE_COMPUTE_BIT;

	return res;
}

constexpr VkShaderStageFlagBits shader_stage_to_vk(ShaderPipelineStage stage)
{
	switch(stage)
	{
	case SHADER_STAGE_VERTEX:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case SHADER_STAGE_FRAGMENT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case SHADER_STAGE_COMPUTE:
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

constexpr std::array<const char*, 1> default_instance_extensions =
{
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};

constexpr std::array<const char*, 3> device_extensions =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_PRESENT_MODE_FIFO_LATEST_READY_EXTENSION_NAME,
	VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME
};

constexpr std::array dynamic_states
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR,
	VK_DYNAMIC_STATE_CULL_MODE,
	VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
	VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
	VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
	VK_DYNAMIC_STATE_DEPTH_CLAMP_ENABLE_EXT
};

constexpr size_t max_bindless_textures = 65536;
constexpr size_t max_bindless_samplers = 32;
constexpr size_t sem_wait_timeout = 1000000000;
constexpr size_t max_shader_stages = 2;
constexpr size_t max_color_attachments = 8;

struct CMDBuf_Info
{
	VkCommandBuffer handle;
	uint64_t timeline;
};

struct CommandPool
{
	VkCommandPool handle;

	static struct pq_compare_cmdbuf_t
	{
		bool operator()(const CMDBuf_Info& lhs, const CMDBuf_Info& rhs)
		{
			return lhs.timeline < rhs.timeline;
		}
	} pq_compare_cmdbuf;
	std::priority_queue<CMDBuf_Info, std::vector<CMDBuf_Info>, pq_compare_cmdbuf_t> buffers;
};	

struct QueueData
{
	VkQueue handle;
	VkSemaphore semaphore;
	uint64_t timeline{0u};

	uint32_t family;
	std::vector<CommandPool> cmd_pools;
};

template <typename T>
struct BindlessResourceHeap
{
	VkDescriptorSetLayout layout;
	VkDescriptorPool pool;
	VkDescriptorSet set;

	std::vector<T> resources;
};

struct GPUBuffer
{
	VkBuffer handle;
	VkDeviceMemory allocation;
	std::byte* mapped;
	size_t size;
};

struct gpu_context_t
{
	VkInstance instance;
	VkPhysicalDevice phys_device;
	VkDevice device;

	std::array<QueueData, 3> queue_data;

	std::vector<GPUBuffer> buffers;

	BindlessResourceHeap<VkImageView> bindless_texture_heap;
	BindlessResourceHeap<VkImageView> bindless_rwtexture_heap;
	BindlessResourceHeap<VkSampler> bindless_sampler_heap;

	GPUTexture default_texture;
	GPUTextureDescriptor default_texture_view;
	GPUTextureDescriptor default_rwtexture_view;
	GPUSampler default_sampler;

	VkSurfaceKHR swapchain_surface;
	VkSwapchainKHR swapchain;
	VkPresentModeKHR swapchain_pmode;

	std::vector<GPUTexture> swapchain_textures;
	std::vector<GPUTextureDescriptor> swapchain_texviews;
	uint32_t current_swapchain_index;
	bool swapchain_dirty;
};

gpu_context_t* gpu_context = nullptr;

std::vector<VkDeviceQueueCreateInfo> vulkan_device_create_queues()
{
	uint32_t qf_count{0};
	vkGetPhysicalDeviceQueueFamilyProperties2(gpu_context->phys_device, &qf_count, nullptr);

	std::vector<VkQueueFamilyProperties2> queue_families(qf_count);
	for(auto& qf : queue_families)
	{
		qf.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		qf.pNext = nullptr;
	}
	
	vkGetPhysicalDeviceQueueFamilyProperties2(gpu_context->phys_device, &qf_count, queue_families.data());

 	auto iter = std::find_if(queue_families.begin(), queue_families.end(),
        [](const VkQueueFamilyProperties2& elem)
        {
                const auto& qfp = elem.queueFamilyProperties;

                return (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                       (qfp.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                       (qfp.queueFlags & VK_QUEUE_TRANSFER_BIT);
        });
        gpu_context->queue_data[0].family = std::distance(queue_families.begin(), iter);

        iter = std::find_if(queue_families.begin(), queue_families.end(),
        [](const VkQueueFamilyProperties2& elem)
        {
                const auto& qfp = elem.queueFamilyProperties;

                return !(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                        (qfp.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                        (qfp.queueFlags & VK_QUEUE_TRANSFER_BIT);
        });
        gpu_context->queue_data[1].family = (iter != queue_families.end()) ? std::distance(queue_families.begin(), iter) : gpu_context->queue_data[0].family;

        iter = std::find_if(queue_families.begin(), queue_families.end(),
        [](const VkQueueFamilyProperties2& elem)
        {
                const auto& qfp = elem.queueFamilyProperties;

                return !(qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                       !(qfp.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                        (qfp.queueFlags & VK_QUEUE_TRANSFER_BIT);
        });
        gpu_context->queue_data[2].family = (iter != queue_families.end()) ? std::distance(queue_families.begin(), iter) : gpu_context->queue_data[0].family;

	std::vector<VkDeviceQueueCreateInfo> queue_ci;

	float queue_priority = 1.0f;
	for(auto& queue : gpu_context->queue_data)
	{
		VkDeviceQueueCreateInfo ci
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.queueFamilyIndex = queue.family,
			.queueCount = 1,
			.pQueuePriorities = &queue_priority
		};

		queue_ci.push_back(ci);
	}

	return queue_ci;
}

void vulkan_device_setup_queues()
{
	const VkSemaphoreTypeCreateInfo sem_type
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0
	};

	const VkSemaphoreCreateInfo sem_ci
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &sem_type
	};

	for(auto& queue: gpu_context->queue_data)
	{
		vkGetDeviceQueue(gpu_context->device, queue.family, 0, &queue.handle);
		vkCreateSemaphore(gpu_context->device, &sem_ci, nullptr, &queue.semaphore);
		queue.timeline = 0;

		queue.cmd_pools.resize(std::thread::hardware_concurrency());
		for(auto& cpool : queue.cmd_pools)
		{
			const VkCommandPoolCreateInfo cpool_ci
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.pNext = nullptr,
				.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
				.queueFamilyIndex = queue.family
			};

			vkCreateCommandPool(gpu_context->device, &cpool_ci, nullptr, &cpool.handle);
		}
	}
}	

bool vulkan_create_device(std::span<VkPhysicalDevice> phys_devices, int index = -1)
{
	bool found = false;
	uint32_t max_score = 0;

	for(int i = 0; i < phys_devices.size(); i++)
	{
		auto phys = phys_devices[i];
		if(index == i)
		{
			found = true;
			gpu_context->phys_device = phys;
			break;
		}

		uint32_t score = 0;
		VkPhysicalDeviceProperties2 props
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = nullptr
		};

		vkGetPhysicalDeviceProperties2(phys, &props);
		switch(props.properties.deviceType)
		{
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			score += 1000;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			score += 100;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			score += 10;
			break;
		default:
			score += 1;
		}

		if(max_score < score)
		{
			max_score = score;
			found = true;
			gpu_context->phys_device = phys;
		}
	}

	if(!found)
	{
		log::error("gpu_vulkan: no suitable devices found");
		return false;
	}
		
	VkPhysicalDeviceProperties2 props
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = nullptr
	};

	vkGetPhysicalDeviceProperties2(gpu_context->phys_device, &props);

	log::info("gpu_vulkan: selected render device {}", std::string_view{props.properties.deviceName});
	auto queue_ci = vulkan_device_create_queues();

	VkPhysicalDeviceExtendedDynamicState3FeaturesEXT ds3ext
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT,
		.pNext = nullptr,
		.extendedDynamicState3DepthClampEnable = true
	};

	VkPhysicalDevicePresentModeFifoLatestReadyFeaturesKHR chain_pmfifo
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_MODE_FIFO_LATEST_READY_FEATURES_KHR,
		.pNext = &ds3ext,
		.presentModeFifoLatestReady = true
	};

	VkPhysicalDeviceRobustness2FeaturesEXT chain_rob2
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
		.pNext = &chain_pmfifo,
		.nullDescriptor = true
	};

	VkPhysicalDeviceVulkan14Features chain_vk14
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
		.pNext = &chain_rob2,
		.indexTypeUint8 = true,
		.maintenance5 = true,
		.pushDescriptor = true
	};

	VkPhysicalDeviceVulkan13Features chain_vk13
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.pNext = &chain_vk14,
		.shaderDemoteToHelperInvocation = true,
		.synchronization2 = true,
		.dynamicRendering = true
	};

	VkPhysicalDeviceVulkan12Features chain_vk12
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &chain_vk13,
		.drawIndirectCount = true,
		.storageBuffer8BitAccess = true,
		.shaderInt8 = true,
		.shaderSampledImageArrayNonUniformIndexing = true,
		.shaderStorageImageArrayNonUniformIndexing = true,
		.descriptorBindingSampledImageUpdateAfterBind = true,
		.descriptorBindingStorageImageUpdateAfterBind = true,
		.descriptorBindingUpdateUnusedWhilePending = true,
		.descriptorBindingPartiallyBound = true,
		.descriptorBindingVariableDescriptorCount = true,
		.runtimeDescriptorArray = true,
		.samplerFilterMinmax = true,
		.scalarBlockLayout = true,
		.hostQueryReset = true,
		.timelineSemaphore = true,
		.bufferDeviceAddress = true,
		.vulkanMemoryModel = true,
		.vulkanMemoryModelDeviceScope = true,
		.subgroupBroadcastDynamicId = true
	};

	VkPhysicalDeviceVulkan11Features chain_vk11
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
		.pNext = &chain_vk12,
		.storageBuffer16BitAccess = true,
		.multiview = true,
		.shaderDrawParameters = true
	};

	VkPhysicalDeviceFeatures2 chain_devf2
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &chain_vk11,
		.features =
		{
			.logicOp = true,
			.multiDrawIndirect = true,
			.drawIndirectFirstInstance = true,
			.depthClamp = true,
			.fillModeNonSolid = true,
			.samplerAnisotropy = true,
			.textureCompressionBC = true,
			.pipelineStatisticsQuery = true,
			.fragmentStoresAndAtomics = true
		}
	};

	const VkDeviceCreateInfo device_ci
	{
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext = &chain_devf2,
		.queueCreateInfoCount = static_cast<uint32_t>(queue_ci.size()),
		.pQueueCreateInfos = queue_ci.data(),
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
		.ppEnabledExtensionNames = device_extensions.data(),
		.pEnabledFeatures = nullptr
	};

	if(vkCreateDevice(gpu_context->phys_device, &device_ci, nullptr, &gpu_context->device) != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to create device");
		return false;
	}

	volkLoadDevice(gpu_context->device);

	vulkan_device_setup_queues();

	return true;
}
	
bool vulkan_context_init()
{
	if(volkInitialize() != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to initialize volk loader");
		return false;
	}

	VkApplicationInfo app_info
	{
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pNext = nullptr,
		.pApplicationName = "penumbra",
		.applicationVersion = 1,
		.pEngineName = "penumbra",
		.engineVersion = 1,
		.apiVersion = VK_API_VERSION_1_4
	};

	std::vector<const char*> instance_extensions;
	for(auto ext : default_instance_extensions)
		instance_extensions.push_back(ext);

	uint32_t sdl_ext_count = 0;
	auto sdl_ext_array = SDL_Vulkan_GetInstanceExtensions(&sdl_ext_count);
	for(uint32_t i = 0; i < sdl_ext_count; i++)
		instance_extensions.push_back(sdl_ext_array[i]);

	const VkInstanceCreateInfo instance_info
	{
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = 0,
		.ppEnabledLayerNames = nullptr,
		.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
		.ppEnabledExtensionNames = instance_extensions.data()
	};

	if(vkCreateInstance(&instance_info, nullptr, &gpu_context->instance) != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to create instance");
		return false;
	}
	volkLoadInstance(gpu_context->instance);

	uint32_t pdev_count;
	vkEnumeratePhysicalDevices(gpu_context->instance, &pdev_count, nullptr);

	if(!pdev_count)
	{
		log::error("gpu_vulkan: no compatible devices present");
		return false;
	}

	std::vector<VkPhysicalDevice> phys_devices(pdev_count);
	vkEnumeratePhysicalDevices(gpu_context->instance, &pdev_count, phys_devices.data());

	std::string devlist_msg;
	for(uint32_t i = 0; i < pdev_count; i++)
	{
		VkPhysicalDeviceProperties2 props
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
			.pNext = nullptr
		};

		vkGetPhysicalDeviceProperties2(phys_devices[i], &props);
		devlist_msg += std::format("\n{}: {}", i, std::string_view{props.properties.deviceName});
	}
	log::info("gpu_vulkan: enumerated render devices: {}", devlist_msg);

	if(vulkan_create_device(phys_devices))
		return true;

	return false;
}

void vulkan_setup_descriptor_heaps()
{
	VkDescriptorBindingFlags bflags{VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bflag_ci
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
		.pNext = nullptr,
		.bindingCount = 1u,
		.pBindingFlags = &bflags
	};
	
	VkDescriptorSetLayoutBinding binding
	{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = max_bindless_textures,
	       	.stageFlags = VK_SHADER_STAGE_ALL,
		.pImmutableSamplers = nullptr	
	};

	VkDescriptorSetLayoutCreateInfo layout_ci
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = &bflag_ci,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
		.bindingCount = 1,
		.pBindings = &binding
	};
	
	VkDescriptorPoolSize dpool
	{
		.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = max_bindless_textures
	};

	VkDescriptorPoolCreateInfo pool_ci
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
		.maxSets = 1u,
		.poolSizeCount = 1u,
		.pPoolSizes = &dpool
	};

	uint32_t max_binding = max_bindless_textures - 1;
	VkDescriptorSetVariableDescriptorCountAllocateInfo vdc_alloc
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorSetCount = 1,
		.pDescriptorCounts = &max_binding
	};

	VkDescriptorSetAllocateInfo alloc_info
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = &vdc_alloc,
		.descriptorSetCount = 1
	};

	vkCreateDescriptorSetLayout(gpu_context->device, &layout_ci, nullptr, &gpu_context->bindless_texture_heap.layout);
	vkCreateDescriptorPool(gpu_context->device, &pool_ci, nullptr, &gpu_context->bindless_texture_heap.pool);
	
	alloc_info.descriptorPool = gpu_context->bindless_texture_heap.pool;
	alloc_info.pSetLayouts = &gpu_context->bindless_texture_heap.layout;
	vkAllocateDescriptorSets(gpu_context->device, &alloc_info, &gpu_context->bindless_texture_heap.set);

	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	dpool.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	vkCreateDescriptorSetLayout(gpu_context->device, &layout_ci, nullptr, &gpu_context->bindless_rwtexture_heap.layout);
	vkCreateDescriptorPool(gpu_context->device, &pool_ci, nullptr, &gpu_context->bindless_rwtexture_heap.pool);
	
	alloc_info.descriptorPool = gpu_context->bindless_rwtexture_heap.pool;
	alloc_info.pSetLayouts = &gpu_context->bindless_rwtexture_heap.layout;
	vkAllocateDescriptorSets(gpu_context->device, &alloc_info, &gpu_context->bindless_rwtexture_heap.set);

	binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	binding.descriptorCount = max_bindless_samplers;
	dpool.type = VK_DESCRIPTOR_TYPE_SAMPLER;
	dpool.descriptorCount = max_bindless_samplers;
	max_binding = max_bindless_samplers - 1;

	vkCreateDescriptorSetLayout(gpu_context->device, &layout_ci, nullptr, &gpu_context->bindless_sampler_heap.layout);
	vkCreateDescriptorPool(gpu_context->device, &pool_ci, nullptr, &gpu_context->bindless_sampler_heap.pool);
	
	alloc_info.descriptorPool = gpu_context->bindless_sampler_heap.pool;
	alloc_info.pSetLayouts = &gpu_context->bindless_sampler_heap.layout;
	vkAllocateDescriptorSets(gpu_context->device, &alloc_info, &gpu_context->bindless_sampler_heap.set);
}

bool gpu_init()
{
	gpu_context = new gpu_context_t();

	if(!vulkan_context_init())
		return false;

	vulkan_setup_descriptor_heaps();
	
	gpu_context->default_texture = gpu_create_texture
	({
	 	.dim = {1u, 1u, 1u}, 
		.format = GPU_FORMAT_RGBA8_UNORM,
		.usage = GPU_TEXTURE_SAMPLED | GPU_TEXTURE_STORAGE
	});

	gpu_context->default_texture_view = gpu_texture_view_descriptor(gpu_context->default_texture, {.format = GPU_FORMAT_RGBA8_UNORM});

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, gpu_context->default_texture_view, GPU_STAGE_NONE, GPU_STAGE_ALL, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	auto sync = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	gpu_wait_queue(GPU_QUEUE_GRAPHICS, sync);

	gpu_context->default_sampler = gpu_create_sampler
	({
		.mag_filter = GPU_FILTER_LINEAR,
		.min_filter = GPU_FILTER_LINEAR,
		.mip_filter = GPU_FILTER_LINEAR,
		.address_mode_u = GPU_ADDRESS_MODE_REPEAT,
		.address_mode_v = GPU_ADDRESS_MODE_REPEAT,
		.address_mode_w = GPU_ADDRESS_MODE_REPEAT
	});

	return true;
}

void gpu_cleanup_swapchain();
void gpu_shutdown()
{
	for(auto res : gpu_context->bindless_texture_heap.resources)
		vkDestroyImageView(gpu_context->device, res, nullptr);
	
	for(auto res : gpu_context->bindless_rwtexture_heap.resources)
		vkDestroyImageView(gpu_context->device, res, nullptr);

	for(auto res : gpu_context->bindless_sampler_heap.resources)
		vkDestroySampler(gpu_context->device, res, nullptr);

	if(gpu_context->swapchain != VK_NULL_HANDLE)
	{
		gpu_cleanup_swapchain();
		vkDestroySurfaceKHR(gpu_context->instance, gpu_context->swapchain_surface, nullptr);
	}

	gpu_destroy_texture(gpu_context->default_texture);

	vkDestroyDescriptorPool(gpu_context->device, gpu_context->bindless_texture_heap.pool, nullptr);
	vkDestroyDescriptorPool(gpu_context->device, gpu_context->bindless_rwtexture_heap.pool, nullptr);
	vkDestroyDescriptorPool(gpu_context->device, gpu_context->bindless_sampler_heap.pool, nullptr);
	vkDestroyDescriptorSetLayout(gpu_context->device, gpu_context->bindless_texture_heap.layout, nullptr);
	vkDestroyDescriptorSetLayout(gpu_context->device, gpu_context->bindless_rwtexture_heap.layout, nullptr);
	vkDestroyDescriptorSetLayout(gpu_context->device, gpu_context->bindless_sampler_heap.layout, nullptr);
	
	for(auto& queue : gpu_context->queue_data)
	{
		for(auto& pool : queue.cmd_pools)
			vkDestroyCommandPool(gpu_context->device, pool.handle, nullptr);

		vkDestroySemaphore(gpu_context->device, queue.semaphore, nullptr);
	}

	vkDestroyDevice(gpu_context->device, nullptr);
	vkDestroyInstance(gpu_context->instance, nullptr);

	delete gpu_context;
}

std::optional<uint32_t> get_memory_type(uint32_t type, VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties2 mp
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
		.pNext = nullptr
	};

	vkGetPhysicalDeviceMemoryProperties2(gpu_context->phys_device, &mp);

	for(uint32_t i = 0; i < mp.memoryProperties.memoryTypeCount; i++)
	{
		if((type & 1) == 1)
		{
			if((mp.memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
				return i;
		}
		type >>= 1;
	}

	return std::nullopt;
}

GPUPointer gpu_allocate_memory(size_t size, GPUMemoryHeap heap, GPUBufferUsage usage)
{
	std::array<uint32_t, 3> indices;
	indices[0] = gpu_context->queue_data[0].family;
	indices[1] = gpu_context->queue_data[1].family;
	indices[2] = gpu_context->queue_data[2].family;

	const VkBufferUsageFlags2CreateInfo usage_ci
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
		.pNext = nullptr,
		.usage = decode_buffer_usage(usage) | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT
	};

	const VkBufferCreateInfo buffer_ci
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = &usage_ci,
		.flags = 0,
		.size = size,
		.usage = 0,
		.sharingMode = VK_SHARING_MODE_CONCURRENT,
		.queueFamilyIndexCount = static_cast<uint32_t>(indices.size()),
		.pQueueFamilyIndices = indices.data()
	};

	VkBuffer buf;
	auto status = vkCreateBuffer(gpu_context->device, &buffer_ci, nullptr, &buf);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to allocate memory: {}", string_VkResult(status));
		return {0, 0};
	}

	VkMemoryRequirements2 mem_req
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
		.pNext = nullptr
	};

	const VkBufferMemoryRequirementsInfo2 req_info
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
		.pNext = nullptr,
		.buffer = buf
	};

	vkGetBufferMemoryRequirements2(gpu_context->device, &req_info, &mem_req);
	auto mem_idx = get_memory_type(mem_req.memoryRequirements.memoryTypeBits, decode_memory_heap(heap));
	if(!mem_idx.has_value())
	{
		log::error("gpu_vulkan: failed to allocate memory: invalid memory heap");
		return {0, 0};
	}

	const VkMemoryAllocateFlagsInfo alloc_flags
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
		.pNext = nullptr,
		.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
	};

	const VkMemoryAllocateInfo alloc_chain
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &alloc_flags,
		.allocationSize = mem_req.memoryRequirements.size,
		.memoryTypeIndex = mem_idx.value()
	};

	VkDeviceMemory mem;
	status = vkAllocateMemory(gpu_context->device, &alloc_chain, nullptr, &mem);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to allocate memory: {}", string_VkResult(status));
		return {0, 0};
	}

	status = vkBindBufferMemory(gpu_context->device, buf, mem, 0);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to allocate memory: {}", string_VkResult(status));
		return {0, 0};
	}

	void* ptr = nullptr;
	if(heap != GPU_MEMORY_PRIVATE)
		vkMapMemory(gpu_context->device, mem, 0, size, 0, &ptr);

	gpu_context->buffers.emplace_back(buf, mem, reinterpret_cast<std::byte*>(ptr), size);
	auto handle = gpu_context->buffers.size();

	return {handle, 0};
}

void gpu_free_memory(GPUPointer& ptr)
{
	assert(ptr.handle);
	auto& buffer = gpu_context->buffers[ptr.handle - 1];

	vkDestroyBuffer(gpu_context->device, buffer.handle, nullptr);
	vkFreeMemory(gpu_context->device, buffer.allocation, nullptr);
}

GPUDevicePointer gpu_host_to_device_pointer(const GPUPointer& ptr)
{
	assert(ptr.handle);
	auto& buffer = gpu_context->buffers[ptr.handle - 1];

	const VkBufferDeviceAddressInfo info
	{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
		.pNext = nullptr,
		.buffer = buffer.handle
	};
	
	return vkGetBufferDeviceAddress(gpu_context->device, &info) + ptr.offset;
}

std::byte* gpu_map_memory(const GPUPointer& ptr)
{
	assert(ptr.handle);
	auto& buffer = gpu_context->buffers[ptr.handle - 1];
	if(!buffer.mapped)
		return nullptr;

	return buffer.mapped + ptr.offset;
}

GPUTexture gpu_create_texture(const GPUTextureDesc& desc)
{
	VkImageCreateFlags img_flags{};
	if(desc.type == GPU_TEXTURE_CUBE)
		img_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	else if(desc.type == GPU_TEXTURE_2D_ARRAY)
		img_flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;	

	VkImage handle;
	const VkImageCreateInfo image_ci
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = img_flags,
		.imageType = image_type_to_vk(desc.type),
		.format = format_to_vk(desc.format),
		.extent = {desc.dim.x, desc.dim.y, desc.dim.z},
		.mipLevels = desc.mip_count,
		.arrayLayers = desc.layer_count,
		.samples = sample_count_to_vk(desc.sample_count),
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = image_usage_to_vk(desc.usage),
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};
	
	const VkPhysicalDeviceImageFormatInfo2 format_info
	{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		.pNext = nullptr,
		.format = image_ci.format,
		.type = image_ci.imageType,
		.tiling = image_ci.tiling,
		.usage = image_ci.usage,
		.flags = image_ci.flags
	};

	VkImageFormatProperties2 format_props
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		.pNext = nullptr
	};

	auto status = vkGetPhysicalDeviceImageFormatProperties2(gpu_context->phys_device, &format_info, &format_props);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to create texture: {}", string_VkResult(status));
		return {0, 0, uvec3{0u}};
	}

	status = vkCreateImage(gpu_context->device, &image_ci, nullptr, &handle);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to create texture: {}", string_VkResult(status));
		return {0, 0, uvec3{0u}};
	}

	VkMemoryRequirements mem_req{};
	vkGetImageMemoryRequirements(gpu_context->device, handle, &mem_req);
	auto mem_type = get_memory_type(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if(!mem_type.has_value())
	{
		log::error("gpu_vulkan: failed to allocate texture memory: invalid memory heap");
		return {0, 0, uvec3{0u}};
	}

	VkDeviceMemory memory;
	const VkMemoryAllocateInfo alloc
	{
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = nullptr,
		.allocationSize = mem_req.size,
		.memoryTypeIndex = mem_type.value()
	};

	status = vkAllocateMemory(gpu_context->device, &alloc, nullptr, &memory);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to allocate texture memory: {}", string_VkResult(status));
		return {0, 0, uvec3{0u}};
	}
	
	status = vkBindImageMemory(gpu_context->device, handle, memory, 0);
	if(status != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to allocate texture memory: {}", string_VkResult(status));
		return {0, 0, uvec3{0u}};
	}

	return
	{
		std::bit_cast<uint64_t>(handle),
		std::bit_cast<uint64_t>(memory),
		desc.dim
	};	
}

void gpu_destroy_texture(GPUTexture& tex)
{
	vkDestroyImage(gpu_context->device, std::bit_cast<VkImage>(tex.handle), nullptr);
	vkFreeMemory(gpu_context->device, std::bit_cast<VkDeviceMemory>(tex.allocation), nullptr);
}

VkImageView create_image_view(const GPUTexture& tex, const GPUViewDesc& desc)
{
	const VkImageViewCreateInfo view_ci
	{
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.image = std::bit_cast<VkImage>(tex.handle),
		.viewType = image_view_type_to_vk(desc.type),
		.format = format_to_vk(desc.format),
		.components = 
		{
			VK_COMPONENT_SWIZZLE_IDENTITY, 
			VK_COMPONENT_SWIZZLE_IDENTITY, 
			VK_COMPONENT_SWIZZLE_IDENTITY, 
			VK_COMPONENT_SWIZZLE_IDENTITY
		},
		.subresourceRange =
		{
			.aspectMask = format_to_vk_aspect(desc.format),
			.baseMipLevel = desc.base_mip,
			.levelCount = desc.mip_count == GPU_ALL_MIPS ? VK_REMAINING_MIP_LEVELS : desc.mip_count,
		       	.baseArrayLayer = desc.base_layer,
			.layerCount = desc.layer_count == GPU_ALL_LAYERS ? VK_REMAINING_ARRAY_LAYERS : desc.layer_count	
		}	
	};

	VkImageView view;
	auto result = vkCreateImageView(gpu_context->device, &view_ci, nullptr, &view);
	if(result != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to create texture descriptor: {}", string_VkResult(result));
		return VK_NULL_HANDLE;
	}

	return view;
}

GPUTextureDescriptor gpu_texture_view_descriptor(const GPUTexture& tex, const GPUViewDesc& desc)
{
	auto view = create_image_view(tex, desc);
	if(view == VK_NULL_HANDLE)
		return {0, 0, nullptr, desc};

	const VkDescriptorImageInfo img_info
	{
		.sampler = VK_NULL_HANDLE,
		.imageView = view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};

	const VkWriteDescriptorSet ds_write
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = gpu_context->bindless_texture_heap.set,
		.dstBinding = 0,
		.dstArrayElement = static_cast<uint32_t>(gpu_context->bindless_texture_heap.resources.size()),
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImageInfo = &img_info,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr
	};

	vkUpdateDescriptorSets(gpu_context->device, 1u, &ds_write, 0u, nullptr);	

	gpu_context->bindless_texture_heap.resources.push_back(view);	
	return GPUTextureDescriptor{static_cast<uint32_t>(gpu_context->bindless_texture_heap.resources.size()) - 1, 0, &tex, desc};
}

GPUTextureDescriptor gpu_rwtexture_view_descriptor(const GPUTexture& tex, const GPUViewDesc& desc)
{
	auto view = create_image_view(tex, desc);
	if(view == VK_NULL_HANDLE)
		return {0, 0, nullptr, desc};

	const VkDescriptorImageInfo img_info
	{
		.sampler = VK_NULL_HANDLE,
		.imageView = view,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL
	};

	const VkWriteDescriptorSet ds_write
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,
		.dstSet = gpu_context->bindless_rwtexture_heap.set,
		.dstBinding = 0,
		.dstArrayElement = static_cast<uint32_t>(gpu_context->bindless_rwtexture_heap.resources.size()),
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		.pImageInfo = &img_info,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr
	};

	vkUpdateDescriptorSets(gpu_context->device, 1u, &ds_write, 0u, nullptr);

	gpu_context->bindless_rwtexture_heap.resources.push_back(view);
	return GPUTextureDescriptor{static_cast<uint32_t>(gpu_context->bindless_rwtexture_heap.resources.size() - 1), GPU_TEXTURE_DESCRIPTOR_RW, &tex, desc};
}

void gpu_destroy_texture_view(GPUTextureDescriptor& view)
{

}

GPUSampler gpu_create_sampler(const GPUSamplerDesc& desc)
{
	const VkSamplerCreateInfo sampler_ci
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.magFilter = filter_to_vk(desc.mag_filter),
		.minFilter = filter_to_vk(desc.min_filter),
		.mipmapMode = filter_to_mipmap_vk(desc.mip_filter),
		.addressModeU = address_mode_to_vk(desc.address_mode_u),
		.addressModeV = address_mode_to_vk(desc.address_mode_v),
		.addressModeW = address_mode_to_vk(desc.address_mode_w),
		.mipLodBias = 0.0f,
		.anisotropyEnable = (desc.max_anisotropy == 0.0f) ? false : true,
		.maxAnisotropy = desc.max_anisotropy,
		.compareEnable = false,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = false
	};


	VkSampler sampler;
	vkCreateSampler(gpu_context->device, &sampler_ci, nullptr, &sampler);
	
	const VkDescriptorImageInfo info
	{
		.sampler = sampler,
		.imageView = VK_NULL_HANDLE
	};

	const VkWriteDescriptorSet ds_write
	{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,	
		.dstSet = gpu_context->bindless_sampler_heap.set,
		.dstBinding = 0,
		.dstArrayElement = static_cast<uint32_t>(gpu_context->bindless_sampler_heap.resources.size()),
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.pImageInfo = &info,
		.pBufferInfo = nullptr,
		.pTexelBufferView = nullptr
	};	
	vkUpdateDescriptorSets(gpu_context->device, 1u, &ds_write, 0u, nullptr);	
	
	gpu_context->bindless_sampler_heap.resources.push_back(sampler);
	return GPUSampler{static_cast<uint32_t>(gpu_context->bindless_sampler_heap.resources.size()) - 1};
}

GPUCommandBuffer gpu_record_commands(GPUQueue queue)
{
	ZoneScoped;
	assert(queue > GPU_QUEUE_INVALID);
	auto& qd = gpu_context->queue_data[queue - 1];
	auto thread = 0u; //FIXME 

	auto& cpool = qd.cmd_pools[thread];
			
	const VkCommandBufferBeginInfo begin_info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		.pInheritanceInfo = nullptr
	};

	VkCommandBuffer cmd;

	if(!cpool.buffers.empty())
	{
		auto& cb_info = cpool.buffers.top();

		uint64_t qtime = 0;
		vkGetSemaphoreCounterValue(gpu_context->device, qd.semaphore, &qtime);
		if(qtime >= cb_info.timeline)
		{
			cmd = cb_info.handle;
			cpool.buffers.pop();

			vkBeginCommandBuffer(cmd, &begin_info);
			return {thread, nullptr, std::bit_cast<uint64_t>(cmd)};
		}
	}

	const VkCommandBufferAllocateInfo alloc_info
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.pNext = nullptr,
		.commandPool = cpool.handle,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount = 1
	};
	vkAllocateCommandBuffers(gpu_context->device, &alloc_info, &cmd);

	vkBeginCommandBuffer(cmd, &begin_info);
	return {thread, nullptr, std::bit_cast<uint64_t>(cmd), {}, {}};
}

uint64_t gpu_submit(GPUQueue queue, GPUCommandBuffer& cmd)
{
	ZoneScoped;
	assert(queue > GPU_QUEUE_INVALID);

	cmd.bound_pipe = nullptr;
	VkCommandBuffer cbuf = std::bit_cast<VkCommandBuffer>(cmd.handle);
	vkEndCommandBuffer(cbuf);

	auto& qd = gpu_context->queue_data[queue - 1];
	++qd.timeline;

	std::array<VkSemaphoreSubmitInfo, 2> wait_signals;
	std::array<VkSemaphoreSubmitInfo, 2> emit_signals;
	uint32_t ws_count = 0u;
	uint32_t es_count = 1u;

	emit_signals[0] = 
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
		.pNext = nullptr,
		.semaphore = qd.semaphore,
		.value = qd.timeline,
		.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
		.deviceIndex = 0
	};

	if(cmd.wait_signal.object)
	{
		assert(ws_count < 2 && "This command buffer is waiting on too many signals");
		wait_signals[ws_count++] =
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = std::bit_cast<VkSemaphore>(cmd.wait_signal.object),
			.value = cmd.wait_signal.value,
			.stageMask = gpu_stage_to_vk(cmd.wait_signal.stage),
			.deviceIndex = 0
		};
	}

	if(cmd.emit_signal.object)
	{
		assert(es_count < 2 && "This command buffer is emitting too many signals");
		emit_signals[es_count++] =
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = std::bit_cast<VkSemaphore>(cmd.emit_signal.object),
			.value = cmd.emit_signal.value,
			.stageMask = gpu_stage_to_vk(cmd.emit_signal.stage),
			.deviceIndex = 0
		};
	}

	const VkCommandBufferSubmitInfo cmd_sub
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
		.pNext = nullptr,
		.commandBuffer = cbuf,
		.deviceMask = 0
	};

	const VkSubmitInfo2 batch
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
		.pNext = nullptr,
		.flags = 0,
		.waitSemaphoreInfoCount = ws_count,
		.pWaitSemaphoreInfos = ws_count > 0 ? wait_signals.data() : nullptr,
		.commandBufferInfoCount = 1u,
		.pCommandBufferInfos = &cmd_sub,
		.signalSemaphoreInfoCount = es_count,
		.pSignalSemaphoreInfos = emit_signals.data()
	};

	if(vkQueueSubmit2(qd.handle, 1u, &batch, nullptr) != VK_SUCCESS)
	{
		log::error("gpu_vulkan: failed to submit command buffers");
	}

	qd.cmd_pools[cmd.thread].buffers.push({cbuf, qd.timeline});
	return qd.timeline;
}

bool gpu_wait_queue(GPUQueue queue, uint64_t timeline)
{
	ZoneScoped;
	assert(queue > GPU_QUEUE_INVALID);

	const VkSemaphoreWaitInfo wait
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
		.pNext = nullptr,
		.flags = 0,
		.semaphoreCount = 1u,
		.pSemaphores = &gpu_context->queue_data[queue - 1].semaphore,
		.pValues = &timeline
	};

	auto result = vkWaitSemaphores(gpu_context->device, &wait, sem_wait_timeout);
	if(result == VK_TIMEOUT)
	{
		uint64_t g_val = 0;
		vkGetSemaphoreCounterValue(gpu_context->device, gpu_context->queue_data[queue - 1].semaphore, &g_val);
		log::error("gpu_vulkan: wait_queue timed out waiting for signal {:#x}, queue is {:#x}", timeline, g_val);
		return false;
	}
	
	return true;
}

void gpu_wait_idle()
{
	ZoneScoped;
	vkDeviceWaitIdle(gpu_context->device);
}

GPUSemaphore gpu_create_semaphore(uint64_t initial_value, GPUSemaphoreType type)
{
	const VkSemaphoreTypeCreateInfo sem_type
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.pNext = nullptr,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = initial_value
	};

	VkSemaphoreCreateInfo sem_ci
	{
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = &sem_type
	};

	if(type == GPU_SEMAPHORE_BINARY)
		sem_ci.pNext = nullptr;

	VkSemaphore sem;
	vkCreateSemaphore(gpu_context->device, &sem_ci, nullptr, &sem);
	return {std::bit_cast<uint64_t>(sem), initial_value};
}

void gpu_destroy_semaphore(GPUSemaphore& sem)
{
	vkDestroySemaphore(gpu_context->device, std::bit_cast<VkSemaphore>(sem.handle), nullptr);
}

GPUSemaphore gpu_get_queue_timeline(GPUQueue queue)
{
	assert(queue > GPU_QUEUE_INVALID);

	return
	{
		std::bit_cast<uint64_t>(gpu_context->queue_data[queue - 1].semaphore),
		gpu_context->queue_data[queue - 1].timeline
	};
}

VkDescriptorSetLayout shader_create_push_descriptor_set(const DescriptorSetLayoutKey& key)
{
	constexpr size_t max_bindings = 16;

        std::array<VkDescriptorSetLayoutBinding, max_bindings> bindings;
        uint32_t num_bindings = 0;

        for(uint32_t i = 0; i < max_bindings; i++)
        {
                VkShaderStageFlags stages;

                if(key.vs_bindings & (1u << i))
                        stages |= VK_SHADER_STAGE_VERTEX_BIT;
                if(key.fs_bindings & (1u << i))
                        stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
                if(key.cs_bindings & (1u << i))
                        stages |= VK_SHADER_STAGE_COMPUTE_BIT;

                if(stages & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_FRAGMENT_BIT))
                        stages = VK_SHADER_STAGE_ALL;

                if(!stages)
                        continue;

                VkDescriptorType type;
                if(key.sampled_image_bindings & (1u << i))
                        type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                else if(key.storage_image_bindings & (1u << i))
                        type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                else if(key.separate_image_bindings & (1u << i))
                        type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                else if(key.sampler_bindings & (1u << i))
                        type = VK_DESCRIPTOR_TYPE_SAMPLER;
                else if(key.uniform_buffer_bindings & (1u << i))
                        type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                else if(key.storage_buffer_bindings & (1u << i))
                        type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                else
                        std::unreachable();

                bindings[num_bindings++] = {i, type, key.binding_arraysize[i], stages, nullptr};
        }

	const VkDescriptorSetLayoutCreateInfo dsl_ci
	{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT,
		.bindingCount = num_bindings,
		.pBindings = bindings.data()
	};

	VkDescriptorSetLayout dsl;
	vkCreateDescriptorSetLayout(gpu_context->device, &dsl_ci, nullptr, &dsl);
	return dsl;
}

std::pair<VkPipelineLayout, uint64_t> shader_create_pipeline_layout(const Shader& shader)
{	
	std::array<VkDescriptorSetLayout, 4> ds_layouts;

	uint32_t shader_num_dsl = 0;

	ds_layouts[shader_num_dsl++] = gpu_context->bindless_texture_heap.layout;
	ds_layouts[shader_num_dsl++] = gpu_context->bindless_rwtexture_heap.layout;
	ds_layouts[shader_num_dsl++] = gpu_context->bindless_sampler_heap.layout;
	
	uint64_t pdsl_handle = 0;
	if(!shader.dsl_keys[3].is_empty())
	{
		ds_layouts[shader_num_dsl++] = shader_create_push_descriptor_set(shader.dsl_keys[3]);
		pdsl_handle = std::bit_cast<uint64_t>(ds_layouts[3]);
	}

	const VkPushConstantRange s_pconst
	{
		.stageFlags = shader_stage_to_vk_flags(ShaderPipelineStage{shader.pconst.stageFlags}),
		.offset = 0,
		.size = shader.pconst.size
	};

	const VkPipelineLayoutCreateInfo layout_ci
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = shader_num_dsl,
		.pSetLayouts = ds_layouts.data(),
		.pushConstantRangeCount = shader.pconst.size ? 1u : 0u,
		.pPushConstantRanges = shader.pconst.size ? &s_pconst : nullptr
	};

	VkPipelineLayout layout;
	vkCreatePipelineLayout(gpu_context->device, &layout_ci, nullptr, &layout);
	return {layout, pdsl_handle};
}

GPUPipeline gpu_create_compute_pipeline(const Shader& shader)
{
	auto [layout, pdsl_handle] = shader_create_pipeline_layout(shader);
	const VkShaderModuleCreateInfo sm_info
	{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = shader.stages[0].spirv.size() * sizeof(uint32_t),
		.pCode = shader.stages[0].spirv.data()
	};

	const VkComputePipelineCreateInfo pipeline_ci
	{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = 
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = &sm_info,
			.flags = 0,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = nullptr,
			.pName = "main",
			.pSpecializationInfo = nullptr
		},
		.layout = layout,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = 0
	};

	VkPipeline pipe;
	vkCreateComputePipelines(gpu_context->device, nullptr, 1u, &pipeline_ci, nullptr, &pipe);

	return
	{
		std::bit_cast<uint64_t>(pipe),
		std::bit_cast<uint64_t>(layout),
		pdsl_handle,
		static_cast<uint32_t>(shader_stage_to_vk_flags(shader.pconst.stageFlags)),
		static_cast<uint32_t>(shader.pconst.size),
		true
	};
}

GPUPipeline gpu_create_graphics_pipeline(const Shader& shader, const GPURasterDesc& raster)
{
	auto [layout, pdsl_handle] = shader_create_pipeline_layout(shader);
	std::array<VkShaderModuleCreateInfo, max_shader_stages> sm_info;
	std::array<VkPipelineShaderStageCreateInfo, max_shader_stages> stages;
	
	uint32_t num_stages = 0;
	for(auto& stage : shader.stages)
	{
		if(stage.spirv.empty())
			break;

		sm_info[num_stages] =
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.codeSize = stage.spirv.size() * sizeof(uint32_t),
			.pCode = stage.spirv.data()
		};

		stages[num_stages] = 
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = &sm_info[num_stages],
			.flags = 0,
			.stage = shader_stage_to_vk(stage.pipeline_stage),
			.module = VK_NULL_HANDLE,
			.pName = "main",
			.pSpecializationInfo = nullptr
		};
		num_stages++;
	}

	const VkPipelineVertexInputStateCreateInfo vtxinput
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.vertexBindingDescriptionCount = 0,
		.vertexAttributeDescriptionCount = 0
	};

	const VkPipelineInputAssemblyStateCreateInfo inputasm
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.topology = raster_topology_to_vk(raster.topology),
		.primitiveRestartEnable = false
	};

	const VkPipelineDynamicStateCreateInfo dyn_st
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
		.pDynamicStates = dynamic_states.data()
	};

	const VkPipelineTessellationStateCreateInfo tess_state
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.patchControlPoints = 1
	};

	const VkPipelineViewportStateCreateInfo viewport
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.viewportCount = 1,
		.scissorCount = 1
	};

	const VkPipelineRasterizationStateCreateInfo raster_state
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizerDiscardEnable = false,
		.polygonMode = raster_polymode_to_vk(raster.polymode),
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = false,
		.lineWidth = 1.0f
	};

	const VkPipelineMultisampleStateCreateInfo multisample
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = false,
		.alphaToCoverageEnable = false,
		.alphaToOneEnable = false
	};

	const VkPipelineDepthStencilStateCreateInfo depthstencil
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthBoundsTestEnable = false,
		.stencilTestEnable = false
	};

	std::array<VkPipelineColorBlendAttachmentState, max_color_attachments> blend_att;
	for(uint32_t i = 0; i < raster.color_targets.size(); i++)
	{
		auto& att = blend_att[i];
		if(raster.blendstate == nullptr)
		{
			att.blendEnable = false;
			att.colorWriteMask = 0xf;
			continue;
		}

		att.blendEnable = true;
		att.srcColorBlendFactor = blend_factor_to_vk(raster.blendstate->src_color_factor);
		att.dstColorBlendFactor = blend_factor_to_vk(raster.blendstate->dst_color_factor);
		att.colorBlendOp = blend_op_to_vk(raster.blendstate->color_op);
		att.srcAlphaBlendFactor = blend_factor_to_vk(raster.blendstate->src_alpha_factor);
		att.dstAlphaBlendFactor = blend_factor_to_vk(raster.blendstate->dst_alpha_factor);
		att.alphaBlendOp = blend_op_to_vk(raster.blendstate->alpha_op);
		att.colorWriteMask = raster.blendstate->color_write_mask;	
	}
	
	const VkPipelineColorBlendStateCreateInfo blend
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.logicOpEnable = false,
		.attachmentCount = static_cast<uint32_t>(raster.color_targets.size()),
		.pAttachments = blend_att.data()
	};

	std::array<VkFormat, max_color_attachments> color_formats;
	uint32_t num_color_formats = 0;
	for(auto fmt : raster.color_targets)
		color_formats[num_color_formats++] = format_to_vk(fmt);

	const VkPipelineRenderingCreateInfo dynamic_rendering
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = num_color_formats,
		.pColorAttachmentFormats = color_formats.data(), 
		.depthAttachmentFormat = format_to_vk(raster.depth_format),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
	};

	const VkGraphicsPipelineCreateInfo pipeline_ci
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &dynamic_rendering,
		.flags = 0,
		.stageCount = num_stages,
		.pStages = stages.data(),
		.pVertexInputState = &vtxinput,
		.pInputAssemblyState = &inputasm,
		.pTessellationState = &tess_state,
		.pViewportState = &viewport,
		.pRasterizationState = &raster_state,
		.pMultisampleState = &multisample,
		.pDepthStencilState = &depthstencil,
		.pColorBlendState = &blend,
		.pDynamicState = &dyn_st,
		.layout = layout,
		.renderPass = VK_NULL_HANDLE,
		.subpass = 0,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = 0
	};

	VkPipeline pipe;
	vkCreateGraphicsPipelines(gpu_context->device, nullptr, 1u, &pipeline_ci, nullptr, &pipe);

	return
	{
		std::bit_cast<uint64_t>(pipe),
		std::bit_cast<uint64_t>(layout),
		pdsl_handle,
		static_cast<uint32_t>(shader_stage_to_vk_flags(shader.pconst.stageFlags)),
		static_cast<uint32_t>(shader.pconst.size),
		false
	};
}

void gpu_destroy_pipeline(GPUPipeline& pipe)
{
	vkDestroyPipeline(gpu_context->device, std::bit_cast<VkPipeline>(pipe.pso), nullptr);
	vkDestroyPipelineLayout(gpu_context->device, std::bit_cast<VkPipelineLayout>(pipe.layout), nullptr);

	if(pipe.pdsl_handle)
		vkDestroyDescriptorSetLayout(gpu_context->device, std::bit_cast<VkDescriptorSetLayout>(pipe.pdsl_handle), nullptr);
}

void gpu_mem_copy(const GPUCommandBuffer& cmd, const GPUPointer& src, const GPUPointer& dst, size_t size)
{
	assert(src.handle);
	assert(dst.handle);
	auto& src_buffer = gpu_context->buffers[src.handle - 1];
	auto& dst_buffer = gpu_context->buffers[dst.handle - 1];
	assert(src.offset + size <= src_buffer.size);
	assert(dst.offset + size <= dst_buffer.size);

	const VkBufferCopy region
	{
		.srcOffset = src.offset,
		.dstOffset = dst.offset,
		.size = size
	};

	vkCmdCopyBuffer(std::bit_cast<VkCommandBuffer>(cmd.handle), src_buffer.handle, dst_buffer.handle, 1, &region);
}

void gpu_copy_to_texture(const GPUCommandBuffer& cmd, const GPUPointer& src, const GPUTextureDescriptor& dst)
{
	assert(src.handle);
	auto& buffer = gpu_context->buffers[src.handle - 1];
	assert(src.offset < buffer.size);
	assert(dst.texture);

	VkBufferImageCopy region
	{
		.bufferOffset = src.offset,
		.imageSubresource = {format_to_vk_aspect(dst.desc.format), dst.desc.base_mip, dst.desc.base_layer, dst.desc.layer_count == GPU_ALL_LAYERS ? VK_REMAINING_ARRAY_LAYERS : dst.desc.layer_count},
		.imageOffset = {0, 0, 0},
		.imageExtent = {dst.texture->size.x, dst.texture->size.y, dst.texture->size.z}
	};

	vkCmdCopyBufferToImage
	(
		std::bit_cast<VkCommandBuffer>(cmd.handle),
		buffer.handle,
		std::bit_cast<VkImage>(dst.texture->handle),
		VK_IMAGE_LAYOUT_GENERAL,
		1u,
		&region
	);
}

void gpu_copy_from_texture(const GPUCommandBuffer& cmd, const GPUTextureDescriptor& src, const GPUPointer& dst)
{

}

void gpu_barrier(const GPUCommandBuffer& cmd, GPUStage src, GPUStage dst, GPUHazard hazards)
{
	VkMemoryBarrier2 barrier;
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.pNext = nullptr;
	
	barrier.srcStageMask = gpu_stage_to_vk(src);
	barrier.dstStageMask = gpu_stage_to_vk(dst);
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = 0;

	if(hazards == GPU_HAZARD_NONE)
	{
		barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
	}

	if(hazards & GPU_HAZARD_INDIRECT_ARGS)
	{
		barrier.srcAccessMask |= VK_ACCESS_2_SHADER_WRITE_BIT;
		barrier.dstAccessMask |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	}

	if(hazards & GPU_HAZARD_DEPTH_STENCIL)
	{
		barrier.srcAccessMask |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask |= (VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
	}

	const VkDependencyInfo dep
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 1u,
		.pMemoryBarriers = &barrier,
		.bufferMemoryBarrierCount = 0u,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = 0u,
		.pImageMemoryBarriers = nullptr
	};
	
	vkCmdPipelineBarrier2(std::bit_cast<VkCommandBuffer>(cmd.handle), &dep);
}

void gpu_texture_layout_transition(const GPUCommandBuffer& cmd, const GPUTextureDescriptor& tex, GPUStage src_stage, GPUStage dst_stage, GPUTextureLayout src_layout, GPUTextureLayout dst_layout, GPUQueue src_queue, GPUQueue dst_queue)
{
	VkImageMemoryBarrier2 barrier;
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.pNext = nullptr;
	barrier.image = std::bit_cast<VkImage>(tex.texture->handle);
	barrier.subresourceRange = 
	{
		format_to_vk_aspect(tex.desc.format), 
		tex.desc.base_mip,
	       	(tex.desc.mip_count == GPU_ALL_MIPS) ? VK_REMAINING_MIP_LEVELS : tex.desc.mip_count,
		tex.desc.base_layer,
		(tex.desc.layer_count == GPU_ALL_LAYERS) ? VK_REMAINING_ARRAY_LAYERS : tex.desc.layer_count
	};	

	barrier.srcStageMask = gpu_stage_to_vk(src_stage);
	barrier.dstStageMask = gpu_stage_to_vk(dst_stage);
	barrier.oldLayout = texlayout_to_vk(src_layout);
	barrier.newLayout = texlayout_to_vk(dst_layout);

	if(src_layout == GPU_TEXTURE_LAYOUT_GENERAL)
		barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;	
	else
		barrier.srcAccessMask = 0;

	if(dst_layout == GPU_TEXTURE_LAYOUT_GENERAL)
		barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	else
		barrier.dstAccessMask = 0;

	if(src_queue != GPU_QUEUE_INVALID)
		barrier.srcQueueFamilyIndex = gpu_context->queue_data[src_queue - 1].family;
	else
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	if(dst_queue != GPU_QUEUE_INVALID)
		barrier.dstQueueFamilyIndex = gpu_context->queue_data[dst_queue - 1].family;
	else
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	const VkDependencyInfo dep
	{
		.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
		.pNext = nullptr,
		.dependencyFlags = 0,
		.memoryBarrierCount = 0u,
		.pMemoryBarriers = nullptr,
		.bufferMemoryBarrierCount = 0u,
		.pBufferMemoryBarriers = nullptr,
		.imageMemoryBarrierCount = 1u,
		.pImageMemoryBarriers = &barrier
	};

	vkCmdPipelineBarrier2(std::bit_cast<VkCommandBuffer>(cmd.handle), &dep);
}

void gpu_wait_signal(GPUCommandBuffer& cmd, GPUStage dst_stage, GPUSemaphore& sem, uint64_t timeline)
{
	assert(!cmd.wait_signal.handle && "Command buffer already waiting on signal");
	cmd.wait_signal =
	{
		std::bit_cast<uint64_t>(sem.handle),
		timeline,
		dst_stage
	};
}

void gpu_emit_signal(GPUCommandBuffer& cmd, GPUStage src_stage, GPUSemaphore& sem, uint64_t timeline)
{
	assert(!cmd.emit_signal.handle && "Command buffer already emitting signal");
	cmd.emit_signal =
	{
		std::bit_cast<uint64_t>(sem.handle),
		timeline, 
		src_stage
	};
}

void gpu_set_pipeline(GPUCommandBuffer& cmd, GPUPipeline& pipe)
{
	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);
	cmd.bound_pipe = &pipe;

	std::array<VkDescriptorSet, 4> sets;
	uint32_t ds_count = 0;

	sets[ds_count++] = gpu_context->bindless_texture_heap.set;
	sets[ds_count++] = gpu_context->bindless_rwtexture_heap.set;
	sets[ds_count++] = gpu_context->bindless_sampler_heap.set;

	if(pipe.pdsl_handle)
		sets[ds_count++] = std::bit_cast<VkDescriptorSet>(pipe.pdsl_handle);

	auto bindpoint = pipe.is_compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
	vkCmdBindPipeline(cb, bindpoint, std::bit_cast<VkPipeline>(pipe.pso));

	vkCmdBindDescriptorSets(cb, bindpoint, std::bit_cast<VkPipelineLayout>(pipe.layout), 0u, ds_count, sets.data(), 0u, nullptr);
}

void gpu_dispatch(const GPUCommandBuffer& cmd, void* data, uvec3 dim)
{
	assert(cmd.bound_pipe);
	assert(cmd.bound_pipe->is_compute);
	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);
   
	if(data && cmd.bound_pipe->pconst_size)
		vkCmdPushConstants(cb, std::bit_cast<VkPipelineLayout>(cmd.bound_pipe->layout), cmd.bound_pipe->pconst_stage, 0, cmd.bound_pipe->pconst_size, data);

	vkCmdDispatch(cb, dim.x, dim.y, dim.z);
}

void gpu_dispatch_indirect(const GPUCommandBuffer& cmd, void* data, const GPUPointer& dim)
{
	assert(cmd.bound_pipe);
	assert(cmd.bound_pipe->is_compute);
	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);
	assert(dim.handle);	
	auto& buffer = gpu_context->buffers[dim.handle - 1];
	assert(dim.offset < buffer.size);

	if(data && cmd.bound_pipe->pconst_size)
		vkCmdPushConstants(cb, std::bit_cast<VkPipelineLayout>(cmd.bound_pipe->layout), cmd.bound_pipe->pconst_stage, 0, cmd.bound_pipe->pconst_size, data);


	vkCmdDispatchIndirect(cb, buffer.handle, dim.offset);
}

constexpr VkImageView image_view_from_descriptor(GPUTextureDescriptor& tex)
{
	if(tex.flags & GPU_TEXTURE_DESCRIPTOR_RW)
		return gpu_context->bindless_rwtexture_heap.resources[tex.handle];
	else
		return gpu_context->bindless_texture_heap.resources[tex.handle];
}

void gpu_begin_renderpass(const GPUCommandBuffer& cmd, const GPURenderPassDesc& rp)
{
	std::array<VkRenderingAttachmentInfo, max_color_attachments> attachments;
	uint32_t att_count = 0;

	for(const auto& att : rp.color_targets)
	{
		assert(att_count < max_color_attachments && "render pass has too many color attachments!");
		if(!att.resource)
			continue;

		auto& new_att = attachments[att_count++];

		new_att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		new_att.pNext = nullptr;
		new_att.imageView = image_view_from_descriptor(*(att.resource));
		new_att.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		new_att.resolveMode = VK_RESOLVE_MODE_NONE;
		new_att.resolveImageView = VK_NULL_HANDLE;
		new_att.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		new_att.loadOp = load_op_to_vk(att.load_op);
	        new_att.storeOp = store_op_to_vk(att.store_op);
		new_att.clearValue.color = {att.clear, att.clear, att.clear, att.clear};	
	}

	VkRenderingAttachmentInfo depth
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = rp.depth_target.resource ? image_view_from_descriptor(*(rp.depth_target.resource)) : VK_NULL_HANDLE,
		.imageLayout = VK_IMAGE_LAYOUT_GENERAL,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.resolveImageView = VK_NULL_HANDLE,
		.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.loadOp = load_op_to_vk(rp.depth_target.load_op),
		.storeOp = store_op_to_vk(rp.depth_target.store_op),
		.clearValue = {.depthStencil={rp.depth_target.clear, 0}}
	};

	VkRect2D render_area =
	{
		{
		static_cast<int32_t>(rp.render_area.x),
		static_cast<int32_t>(rp.render_area.y)
		},
		{rp.render_area.z, rp.render_area.w}
	};

	VkRenderingInfo render_info
	{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = nullptr,
		.flags = 0,
		.renderArea = render_area, 
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = att_count,
		.pColorAttachments = attachments.data(),
		.pDepthAttachment = (depth.imageView != VK_NULL_HANDLE) ? &depth : nullptr,
		.pStencilAttachment = nullptr
	};

	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);

	vkCmdBeginRendering(cb, &render_info);

	VkViewport viewport
	{
		static_cast<float>(rp.render_area.x),
		static_cast<float>(rp.render_area.y),
		static_cast<float>(rp.render_area.z), 
		static_cast<float>(rp.render_area.w),
	       	0.0f, 1.0f
	};
	
	vkCmdSetViewport(cb, 0u, 1u, &viewport);
	vkCmdSetScissor(cb, 0u, 1u, &render_area);
	
	vkCmdSetCullMode(cb, VK_CULL_MODE_NONE);
	vkCmdSetDepthCompareOp(cb, VK_COMPARE_OP_ALWAYS);
	vkCmdSetDepthTestEnable(cb, false);
	vkCmdSetDepthWriteEnable(cb, false);
	vkCmdSetDepthClampEnableEXT(cb, false);
}

void gpu_end_renderpass(const GPUCommandBuffer& cmd)
{
	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);
	vkCmdEndRendering(cb);
}

void gpu_set_scissor(const GPUCommandBuffer& cmd, uvec4 scissor)
{
	VkRect2D vkscissor
	{
		{
		static_cast<int32_t>(scissor.x),
		static_cast<int32_t>(scissor.y)
		},
		{scissor.z, scissor.w}
	};

	vkCmdSetScissor(std::bit_cast<VkCommandBuffer>(cmd.handle), 0u, 1u, &vkscissor);
}

void gpu_set_cull_mode(const GPUCommandBuffer& cmd, GPUCullMode mode)
{
	vkCmdSetCullMode(std::bit_cast<VkCommandBuffer>(cmd.handle), raster_cullmode_to_vk(mode)); 
}

void gpu_bind_index_buffer(const GPUCommandBuffer& cmd, const GPUPointer& ibuf, GPUIndexType type)
{
	assert(ibuf.handle);
	auto& buffer = gpu_context->buffers[ibuf.handle - 1];
	assert(ibuf.offset < buffer.size);

	vkCmdBindIndexBuffer(std::bit_cast<VkCommandBuffer>(cmd.handle), buffer.handle, ibuf.offset, index_type_to_vk(type));	
}

void gpu_draw(const GPUCommandBuffer& cmd, void* data, uint32_t vertex_count, uint32_t instance_count, uint32_t base_vertex, uint32_t base_instance)
{
	assert(cmd.bound_pipe);
	assert(!cmd.bound_pipe->is_compute);
	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);

	if(data && cmd.bound_pipe->pconst_size)
		vkCmdPushConstants(cb, std::bit_cast<VkPipelineLayout>(cmd.bound_pipe->layout), cmd.bound_pipe->pconst_stage, 0, cmd.bound_pipe->pconst_size, data);

	vkCmdDraw(cb, vertex_count, instance_count, base_vertex, base_instance);
}

void gpu_draw_indexed(const GPUCommandBuffer& cmd, void* data, uint32_t index_count, uint32_t instance_count, uint32_t base_index, uint32_t base_vertex, uint32_t base_instance)
{
	assert(cmd.bound_pipe);
	assert(!cmd.bound_pipe->is_compute);
	auto cb = std::bit_cast<VkCommandBuffer>(cmd.handle);

	if(data && cmd.bound_pipe->pconst_size)
		vkCmdPushConstants(cb, std::bit_cast<VkPipelineLayout>(cmd.bound_pipe->layout), cmd.bound_pipe->pconst_stage, 0, cmd.bound_pipe->pconst_size, data);

	vkCmdDrawIndexed(cb, index_count, instance_count, base_index, base_vertex, base_instance);
}

VkSurfaceFormatKHR choose_swapchain_format(std::span<VkSurfaceFormatKHR> formats)
{
	for(const auto& format : formats)
	{
		if(format.format == VK_FORMAT_B8G8R8A8_SRGB &&
		   format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	log::warn("gpu_vulkan: requested swapchain format BGRA8_SRGB unsupported, using fallback format");
	return formats[0];
}

VkExtent2D find_swapchain_extent(const VkSurfaceCapabilitiesKHR& caps)
{
	VkExtent2D real_extent = caps.currentExtent;

	real_extent.width = std::max
	(
		caps.minImageExtent.width,
		std::min(caps.maxImageExtent.width, real_extent.width)
	);

	real_extent.height = std::max
	(
		caps.minImageExtent.height,
		std::min(caps.maxImageExtent.height, real_extent.height)
	);

	return real_extent;
}

uint32_t determine_image_count(const VkSurfaceCapabilitiesKHR& caps)
{
	uint32_t count = caps.minImageCount + 1;
	if(caps.maxImageCount > 0 && count > caps.maxImageCount)
		count = caps.maxImageCount;

	return count;
}

void gpu_create_swapchain()
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu_context->phys_device, gpu_context->swapchain_surface, &capabilities);

	uint32_t fmt_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_context->phys_device, gpu_context->swapchain_surface, &fmt_count, nullptr);
	std::vector<VkSurfaceFormatKHR> formats(fmt_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu_context->phys_device, gpu_context->swapchain_surface, &fmt_count, formats.data());
	
	uint32_t pmode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_context->phys_device, gpu_context->swapchain_surface, &pmode_count, nullptr);
	std::vector<VkPresentModeKHR> present_modes(pmode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(gpu_context->phys_device, gpu_context->swapchain_surface, &pmode_count, present_modes.data());

	VkSurfaceFormatKHR format = choose_swapchain_format(formats);	
	VkExtent2D extent = find_swapchain_extent(capabilities);
	gpu_context->swapchain_pmode = VK_PRESENT_MODE_FIFO_KHR; 

	VkSwapchainCreateInfoKHR swapchain_ci
	{
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext = nullptr,
		.surface = gpu_context->swapchain_surface,
		.minImageCount = determine_image_count(capabilities),
		.imageFormat = format.format,
		.imageColorSpace = format.colorSpace,
		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = gpu_context->swapchain_pmode,
		.clipped = true,
		.oldSwapchain = VK_NULL_HANDLE
	};

	vkCreateSwapchainKHR(gpu_context->device, &swapchain_ci, nullptr, &gpu_context->swapchain);

	uint32_t image_count{0};
	vkGetSwapchainImagesKHR(gpu_context->device, gpu_context->swapchain, &image_count, nullptr);
	std::vector<VkImage> swapchain_images(image_count);
	vkGetSwapchainImagesKHR(gpu_context->device, gpu_context->swapchain, &image_count, swapchain_images.data());

	gpu_context->swapchain_textures.reserve(image_count);
	gpu_context->swapchain_texviews.reserve(image_count);

	for(auto img : swapchain_images)
	{
		gpu_context->swapchain_textures.emplace_back(std::bit_cast<uint64_t>(img), 0, uvec3{extent.width, extent.height, 1u});
		gpu_context->swapchain_texviews.push_back(gpu_texture_view_descriptor(gpu_context->swapchain_textures.back(), {.format = GPU_FORMAT_BGRA8_SRGB}));
	}
	
}

void gpu_cleanup_swapchain()
{
	gpu_context->swapchain_textures.clear();
	for(auto& view : gpu_context->swapchain_texviews)
	{
		gpu_destroy_texture_view(view);
	}
	gpu_context->swapchain_texviews.clear();

	vkDestroySwapchainKHR(gpu_context->device, gpu_context->swapchain, nullptr);
}

void gpu_swapchain_init(Window& wnd)
{
	if(!SDL_Vulkan_CreateSurface(wnd.native_handle(), gpu_context->instance, nullptr, &gpu_context->swapchain_surface))
	{
		log::error("gpu_vulkan: failed to create surface: {}", SDL_GetError());
		return;
	}

	gpu_create_swapchain();
	gpu_context->swapchain_dirty = false;
}

GPUTextureDescriptor* gpu_swapchain_acquire_next(GPUSemaphore& sem)
{
	ZoneScoped;
	uint32_t image_index{0};
	
	do
	{
		auto result = vkAcquireNextImageKHR(gpu_context->device, gpu_context->swapchain, 1000000, std::bit_cast<VkSemaphore>(sem.handle), nullptr, &image_index);
	
		if(result == VK_SUCCESS)
		{
			gpu_context->current_swapchain_index = image_index;
			gpu_context->swapchain_dirty = false;
		}
		else if(result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			gpu_context->swapchain_dirty = true;
			gpu_wait_idle();
			gpu_cleanup_swapchain();
			gpu_create_swapchain();
		}
		else
		{
			log::warn("vkAcquireNextImageKHR returned {}", string_VkResult(result));
		}

	} while(gpu_context->swapchain_dirty);

	return &gpu_context->swapchain_texviews[image_index];
}

void gpu_swapchain_present(GPUQueue queue, GPUSemaphore& sem)
{
	ZoneScoped;
	assert(queue > GPU_QUEUE_INVALID);
	VkSemaphore present_sem = std::bit_cast<VkSemaphore>(sem.handle);

	VkPresentInfoKHR present_info
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1u,
		.pWaitSemaphores = &present_sem,
		.swapchainCount = 1u,
		.pSwapchains = &gpu_context->swapchain,
		.pImageIndices = &gpu_context->current_swapchain_index,
		.pResults = nullptr
	};

	auto result = vkQueuePresentKHR(gpu_context->queue_data[queue - 1].handle, &present_info);
	if(result == VK_ERROR_OUT_OF_DATE_KHR || gpu_context->swapchain_dirty)
	{
		gpu_wait_idle();
		gpu_cleanup_swapchain();
		gpu_create_swapchain();
		gpu_context->swapchain_dirty = false;
		return;
	}

	if(result != VK_SUCCESS)
		log::warn("vkQueuePresentKHR returned {}", string_VkResult(result));
}

bool gpu_swapchain_set_present_mode()
{
	return false;
}

}
