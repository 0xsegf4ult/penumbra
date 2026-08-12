#include <penumbra/shader.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/panic.hpp>
#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

#include <array>
#include <expected>
#include <format>
#include <string>

namespace penumbra
{

std::expected<ShaderIR, std::string_view> try_load_shader(const vfs_path& path)
{
	ShaderIR res;

	vfs_fd shader_file = vfs_open(path, VFS_ACCESS_READ);
	if(shader_file < 0)
		return std::unexpected("could not open shader");

	const u8* shader_data = vfs_map(shader_file);
	const auto* header = reinterpret_cast<const ShaderFileFormat::Header*>(shader_data);
	if(header->magic != ShaderFileFormat::fmt_magic || header->vmajor != ShaderFileFormat::fmt_major)
	{
		vfs_close(shader_file);
		return std::unexpected("invalid shader");
	}

	res.cbuffer.stage_flags = header->cbuffer_stages;
	res.cbuffer.size = header->cbuffer_size;

	res.pconst.stage_flags = header->pcb_stages;
	res.pconst.size = header->pcb_size;

	const auto* stages = reinterpret_cast<const ShaderFileFormat::Stage*>(shader_data + sizeof(ShaderFileFormat::Header));
	for(u32 i = 0; i < header->num_stages; i++)
	{
		const auto& stage = stages[i];
		res.stages[i].pipeline_stage = stage.stage;
		res.stages[i].spirv.resize(stage.code_size / sizeof(u32));
		std::memcpy(res.stages[i].spirv.data(), shader_data + stage.code_offset, stage.code_size);
	}

	vfs_close(shader_file);
	return res;
}

ShaderIR load_shader(const vfs_path& path)
{
	auto res = try_load_shader(path);
	if(!res.has_value())
	{
		auto msg = std::format("gpu: failed to load shader[{}]: {}", path.string(), res.error());
		panic(msg.c_str());
	}
	
	return *res;
}

}
