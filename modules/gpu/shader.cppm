export module penumbra.gpu:shader;

import penumbra.core;
import std;

using std::uint8_t, std::uint16_t, std::uint32_t, std::size_t;

namespace penumbra
{

export struct DescriptorSetLayoutKey
{
	static constexpr size_t max_bindings = 16;

	uint16_t sampled_image_bindings{0};
	uint16_t storage_image_bindings{0};
	uint16_t separate_image_bindings{0};
	uint16_t sampler_bindings{0};
	uint16_t uniform_buffer_bindings{0};
	uint16_t storage_buffer_bindings{0};
	uint16_t vs_bindings{0};
	uint16_t fs_bindings{0};
	uint16_t cs_bindings{0};
	uint16_t variable_bindings{0};

	uint8_t binding_arraysize[16];

	[[nodiscard]] constexpr bool is_empty() const noexcept
	{
		return sampled_image_bindings == 0 && storage_image_bindings == 0 && separate_image_bindings == 0 && sampler_bindings == 0 && uniform_buffer_bindings == 0 && storage_buffer_bindings == 0;
	}
	
	bool operator==(const DescriptorSetLayoutKey& other) const noexcept
	{
		return std::memcmp(this, &other, sizeof(DescriptorSetLayoutKey)) == 0;
	}
};

export enum ShaderPipelineStage : uint32_t
{
	SHADER_STAGE_INVALID = 0,
	SHADER_STAGE_VERTEX = 0x1,
	SHADER_STAGE_FRAGMENT = 0x2,
	SHADER_STAGE_COMPUTE = 0x4
};

export struct ShaderFileFormat
{
	constexpr static uint32_t fmt_magic = 0x4c48534c;
	constexpr static uint32_t fmt_major_version = 1u;
	constexpr static uint32_t fmt_minor_version = 0u;

	struct Header
	{
		uint32_t magic{fmt_magic};
		uint32_t vmajor{fmt_major_version};
		uint32_t vminor{fmt_minor_version};
		uint32_t num_dslkeys;
		uint32_t pcb_size;
		uint32_t pcb_stages;
		uint32_t num_stages;
	};

	struct ShaderStage
	{
		uint32_t stage;
		uint32_t code_size;
		uint32_t code_offset;
	};
};

export struct Shader
{
	constexpr static size_t max_shader_stages = 2;
	struct ShaderStage
	{
		std::vector<uint32_t> spirv;
		ShaderPipelineStage pipeline_stage{0};
	};
	std::array<ShaderStage, max_shader_stages> stages;

	std::array<DescriptorSetLayoutKey, 4> dsl_keys{};
	struct PushConstantRange
	{
		ShaderPipelineStage stageFlags;
		uint32_t size;
	} pconst;
};

export std::expected<Shader, std::string_view> load_shader(const vfs::path& path)
{
	log::info("gpu_shader: loading shader {}", path.string());

	Shader res;
	
	auto shader_file = vfs::open(path, vfs::access_readonly);
	if(!shader_file.has_value())
		return std::unexpected("file not found");

	const auto* shader_data = vfs::map<std::byte>(*shader_file, vfs::access_readonly);
	const auto* header = reinterpret_cast<const ShaderFileFormat::Header*>(shader_data);
	if(header->magic != ShaderFileFormat::fmt_magic || header->vmajor != ShaderFileFormat::fmt_major_version)
		return std::unexpected("invalid file");

	res.pconst.stageFlags = ShaderPipelineStage{header->pcb_stages};
	res.pconst.size = header->pcb_size;

	const auto* stages = reinterpret_cast<const ShaderFileFormat::ShaderStage*>(shader_data + sizeof(ShaderFileFormat::Header));
	for(uint32_t i = 0; i < header->num_stages; i++)
	{
		const auto& stage = stages[i];
		res.stages[i].pipeline_stage = ShaderPipelineStage{stage.stage};
		res.stages[i].spirv.resize(stage.code_size / sizeof(uint32_t));
		std::memcpy(res.stages[i].spirv.data(), shader_data + stage.code_offset, stage.code_size);
	}

	const auto* dsl_keys = reinterpret_cast<const std::byte*>(shader_data + sizeof(ShaderFileFormat::Header) + sizeof(ShaderFileFormat::ShaderStage) * header->num_stages);
	for(uint32_t i = 0; i < header->num_dslkeys; i++)
	{
		std::memcpy(&res.dsl_keys[i], dsl_keys, sizeof(DescriptorSetLayoutKey));
		dsl_keys += sizeof(DescriptorSetLayoutKey);
	}

	return res;
}

}
