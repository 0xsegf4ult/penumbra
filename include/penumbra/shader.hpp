#pragma once

#include <penumbra/vfs.hpp>
#include <penumbra/types.hpp>

#include <expected>
#include <string>
#include <vector>

namespace penumbra
{

struct ShaderFileFormat
{
	constexpr static u32 fmt_magic = 0x4c485350;
	constexpr static u32 fmt_major = 2u;
	constexpr static u32 fmt_minor = 0u;

	struct Header
	{
		u32 magic{fmt_magic};
		u32 vmajor{fmt_major};
		u32 vminor{fmt_minor};
		u32 cbuffer_stages{0};
		u32 cbuffer_size{0};
		u32 pcb_stages{0};
		u32 pcb_size{0};
		u32 num_stages{0};
	};

	struct Stage
	{
		u32 stage;
		u32 code_size;
		u32 code_offset;
	};
};

struct ShaderIR
{
	constexpr static size_t max_stages = 2;
	struct Stage
	{
		std::vector<u32> spirv;
		u32 pipeline_stage{0u};
	};
	std::array<Stage, max_stages> stages;

	struct CBufferInfo
	{
		u32 stage_flags{0u};
		u32 size{0u};
	} cbuffer;

	struct PushConstantRange
	{
		u32 stage_flags{0u};
		u32 size{0u};
	} pconst;
};

std::expected<ShaderIR, std::string_view> try_load_shader(const vfs_path& path);
ShaderIR load_shader(const vfs_path& path);

}
