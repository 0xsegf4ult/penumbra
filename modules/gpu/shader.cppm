export module penumbra.gpu:shader;

import penumbra.core;
import std;

using std::uint8_t, std::uint16_t, std::uint32_t, std::size_t;

namespace penumbra
{

export enum ShaderPipelineStage : uint32_t
{
	SHADER_STAGE_INVALID = 0,
	SHADER_STAGE_VERTEX = 0x1,
	SHADER_STAGE_FRAGMENT = 0x2,
	SHADER_STAGE_COMPUTE = 0x4
};

export struct ShaderFileFormat
{
	constexpr static uint32_t fmt_magic = 0x4c485350;
	constexpr static uint32_t fmt_major_version = 2u;
	constexpr static uint32_t fmt_minor_version = 0u;

	struct Header
	{
		uint32_t magic{fmt_magic};
		uint32_t vmajor{fmt_major_version};
		uint32_t vminor{fmt_minor_version};
		uint32_t cbuffer_stages{SHADER_STAGE_INVALID};
		uint32_t cbuffer_size{0};
		uint32_t pcb_stages;
		uint32_t pcb_size;
		uint32_t num_stages;
	};

	struct ShaderStage
	{
		uint32_t stage;
		uint32_t code_size;
		uint32_t code_offset;
	};
};

export struct ShaderIR
{
	constexpr static size_t max_shader_stages = 2;
	struct ShaderStage
	{
		std::vector<uint32_t> spirv;
		ShaderPipelineStage pipeline_stage{0};
	};
	std::array<ShaderStage, max_shader_stages> stages;

	struct CBufferInfo
	{
		ShaderPipelineStage stageFlags;
		uint32_t size;
	} cbuffer;

	struct PushConstantRange
	{
		ShaderPipelineStage stageFlags;
		uint32_t size;
	} pconst;
};

export std::expected<ShaderIR, std::string_view> try_load_shader(const vfs::path& path)
{
	log::info("gpu_shader: loading shader {}", path.string());

	ShaderIR res;
	
	auto shader_file = vfs::open(path, vfs::access_readonly);
	if(!shader_file.has_value())
		return std::unexpected("file not found");

	const auto* shader_data = vfs::map<std::byte>(*shader_file, vfs::access_readonly);
	const auto* header = reinterpret_cast<const ShaderFileFormat::Header*>(shader_data);
	if(header->magic != ShaderFileFormat::fmt_magic || header->vmajor != ShaderFileFormat::fmt_major_version)
		return std::unexpected("invalid file");

	res.cbuffer.stageFlags = ShaderPipelineStage{header->cbuffer_stages};
	res.cbuffer.size = header->cbuffer_size;

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

	return res;
}

export ShaderIR load_shader(const vfs::path& path)
{
	auto res = try_load_shader(path);
	if(!res.has_value())
	{
		auto msg = std::format("gpu: failed to load shader [{}]: {}", path.string(), res.error());
		panic(msg.c_str());
	}

	return *res;
}

}
