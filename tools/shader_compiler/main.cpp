#include "slang-com-ptr.h"
#include "slang.h"

import std;
import penumbra.gpu;

using std::uint32_t, std::size_t;
using Slang::ComPtr;
using namespace std::literals::string_view_literals;

struct CompileContext
{
	uint32_t cbuffer_stages = 0;
	uint32_t cbuffer_size = 0;
	uint32_t pconst_stages = 0;
	uint32_t pconst_size = 0;

	std::array<ComPtr<slang::IEntryPoint>, 3> stage_eps;
	std::array<ComPtr<slang::IComponentType>, 3> stage_pgms;
	std::array<ComPtr<slang::IBlob>, 3> stage_code;
	std::array<slang::IMetadata*, 3> stage_meta;
	std::array<uint32_t, 3> ep_types;
	uint32_t ep_count = 0;
};

void reflect_parameter(uint32_t space, size_t offset, slang::VariableLayoutReflection* param, slang::BindingType btype, CompileContext& ctx)
{
	auto ptype = param->getTypeLayout();
       	switch(ptype->getKind())
	{
	using enum slang::TypeReflection::Kind;
	case ConstantBuffer:
	{
		ptype = ptype->getElementTypeLayout();
		ctx.cbuffer_size = ptype->getSize();

		std::print("ConstantBuffer<{}> size {}", ptype->getType()->getName(), ctx.cbuffer_size);
		break;
	}
	default:
		std::print("Unsupported resource");
	}

	std::print(" stages");

	auto pc = static_cast<SlangParameterCategory>(param->getCategoryByIndex(0));
	for(uint32_t i = 0; i < ctx.ep_count; i++)
	{
		bool is_used = false;
		ctx.stage_meta[i]->isParameterLocationUsed(pc, space, offset, is_used);
		if(is_used)
		{
			if(ctx.ep_types[i] == 0)
			{
				std::print(" VS");
				ctx.cbuffer_stages |= penumbra::SHADER_STAGE_VERTEX;
			}
			else if(ctx.ep_types[i] == 1)
			{
				std::print(" FS");
				ctx.cbuffer_stages |= penumbra::SHADER_STAGE_FRAGMENT;
			}
			else if(ctx.ep_types[i] == 2)
			{
				std::print(" CS");
				ctx.cbuffer_stages |= penumbra::SHADER_STAGE_COMPUTE;
			}
		}
	}	
}	

int main(int argc, const char** argv)
{
	if(argc < 3)
	{
		std::println("Usage: shader_compiler [INPUT] [OUTPUT]");
		return 0;
	}

	std::filesystem::path input_path{argv[1]};
	std::filesystem::path output_path{argv[2]};

	ComPtr<slang::IGlobalSession> global_session;
	createGlobalSession(global_session.writeRef());

	slang::TargetDesc target_desc;
	target_desc.format = SLANG_SPIRV;
	target_desc.profile = global_session->findProfile("spirv_1_6");
	target_desc.forceGLSLScalarBufferLayout = true;

	std::array<slang::CompilerOptionEntry, 5> options =
	{
		slang::CompilerOptionEntry 
		{
			slang::CompilerOptionName::MatrixLayoutRow,
			{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
		},
		slang::CompilerOptionEntry 
		{
			slang::CompilerOptionName::Optimization,
			{slang::CompilerOptionValueKind::Int, 2, 0, nullptr, nullptr}
		},
		slang::CompilerOptionEntry
		{
			slang::CompilerOptionName::DebugInformation,
			{slang::CompilerOptionValueKind::Int, 2, 0, nullptr, nullptr}
		},
		slang::CompilerOptionEntry
		{
			slang::CompilerOptionName::Capability,
			{
				.kind = slang::CompilerOptionValueKind::String,
				.stringValue0 = "vk_mem_model"
			}
		},
		slang::CompilerOptionEntry
		{
			slang::CompilerOptionName::BindlessSpaceIndex,
			{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
		}
	};


	ComPtr<slang::ISession> session;
	slang::SessionDesc session_desc
	{
		.targets = &target_desc,
		.targetCount = 1,
		.compilerOptionEntries = options.data(),
		.compilerOptionEntryCount = options.size()
	};
	global_session->createSession(session_desc, session.writeRef());

	slang::IModule* slang_module = nullptr;
	ComPtr<slang::IBlob> diag_blob;
	slang_module = session->loadModule(input_path.c_str(), diag_blob.writeRef());
	if(diag_blob)
		std::println("{}", reinterpret_cast<const char*>(diag_blob->getBufferPointer()));
	if(!slang_module)
		return 1;

	CompileContext ctx;

	auto check_stage = [slang_module, &session, &ctx](const char* entry, uint32_t eptype) -> bool
	{
		slang_module->findEntryPointByName(entry, ctx.stage_eps[ctx.ep_count].writeRef());
		if(!ctx.stage_eps[ctx.ep_count])
			return false;

		slang::IComponentType* component_types[] = {slang_module, ctx.stage_eps[ctx.ep_count]};

		ComPtr<slang::IBlob> pgm_diag;
		SlangResult result = session->createCompositeComponentType(component_types, 2, ctx.stage_pgms[ctx.ep_count].writeRef(), pgm_diag.writeRef());
		if(pgm_diag)
			std::println("{}", reinterpret_cast<const char*>(pgm_diag->getBufferPointer()));
		
		if(result != 0)
			return false;
		
		ComPtr<slang::IBlob> spirv_diag;
		SlangResult spv_res = ctx.stage_pgms[ctx.ep_count]->getEntryPointCode(0, 0, ctx.stage_code[ctx.ep_count].writeRef(), spirv_diag.writeRef());
		if(spirv_diag)
			std::println("{}", reinterpret_cast<const char*>(spirv_diag->getBufferPointer()));
		if(spv_res != 0)
			return false;

		ctx.stage_pgms[ctx.ep_count]->getEntryPointMetadata(0, 0, &ctx.stage_meta[ctx.ep_count]);
		ctx.ep_types[ctx.ep_count] = eptype;

		ctx.ep_count++;
		return true;
	};

	check_stage("vertexMain", 0);
	check_stage("fragmentMain", 1);
	check_stage("computeMain", 2);
	
	if(ctx.ep_count == 0)
	{
		std::println("Shader has no valid entry points");
		return 1;
	}
	
	slang::ProgramLayout* pgm_layout = ctx.stage_pgms[0]->getLayout();
	std::println("pgm has {} parameters", pgm_layout->getParameterCount());
	for(uint32_t i = 0; i < pgm_layout->getParameterCount(); i++)
	{
		auto param = pgm_layout->getParameterByIndex(i);
		if(param->getCategoryCount() == 0)
			continue; 

		auto category = static_cast<SlangParameterCategory>(param->getCategoryByIndex(0));
		auto index = param->getOffset(category);
		auto space = param->getBindingSpace(category);
		if(category == SLANG_PARAMETER_CATEGORY_SUB_ELEMENT_REGISTER_SPACE)
		{
			std::println("parameter {} - {} is register space {} ", i, param->getName(), index);
			auto rs_typelayout = param->getTypeLayout();
			if(rs_typelayout->getKind() == slang::TypeReflection::Kind::ParameterBlock)
			{
				auto elem_typelayout = rs_typelayout->getElementTypeLayout();
				if(elem_typelayout->getKind() == slang::TypeReflection::Kind::Struct)
				{
					for(uint32_t field = 0; field < elem_typelayout->getFieldCount(); field++)
					{
						auto field_varlayout = elem_typelayout->getFieldByIndex(field);
						auto category = static_cast<SlangParameterCategory>(field_varlayout->getCategoryByIndex(0));
						auto binding = field_varlayout->getOffset(category);
						if(category == SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT)
						{

							std::print("space {} binding {} - {} is ", index, binding, field_varlayout->getName());
							reflect_parameter(index, binding, field_varlayout, elem_typelayout->getBindingRangeType(field), ctx);
							std::println();
						}
						else
							std::println("field {} is not descriptor", field);
					}	
				}
			}
			else
			{
				std::println("invalid register space type");
			}

		}
		else if(category == SLANG_PARAMETER_CATEGORY_DESCRIPTOR_TABLE_SLOT)
		{
			std::print("space {} binding {} - {} is ", space, index, param->getName());
			reflect_parameter(space, index, param, pgm_layout->getGlobalParamsTypeLayout()->getBindingRangeType(i), ctx); 
			std::println();
		}	
		else if(category == SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER)
		{
			auto ptype = param->getTypeLayout();
			auto size = ptype->getSize(category);

			if(ptype->getKind() == slang::TypeReflection::Kind::ConstantBuffer)
			{
				ptype = ptype->getElementTypeLayout();
				size = ptype->getSize();
			}

			std::println("parameter {} - {} is PushConstant<{}> size {}", index, param->getName(), ptype->getType()->getName(), size);
			if(size > 64)
				std::println("PushConstant larger than 64 bytes");

			ctx.pconst_size = std::max(ctx.pconst_size, static_cast<uint32_t>(size));
			for(uint32_t ep = 0; ep < ctx.ep_count; ep++)
			{
				if(ctx.ep_types[ep] == 0)
					ctx.pconst_stages |= penumbra::SHADER_STAGE_VERTEX;
				else if(ctx.ep_types[ep] == 1)
					ctx.pconst_stages |= penumbra::SHADER_STAGE_FRAGMENT;
				else if(ctx.ep_types[ep] == 2)
					ctx.pconst_stages |= penumbra::SHADER_STAGE_COMPUTE;
			}
		}
	}

	for(uint32_t ep = 0; ep < ctx.ep_count; ep++)
	{
		slang::EntryPointReflection* ep_layout = ctx.stage_pgms[ep]->getLayout()->getEntryPointByIndex(0);
		uint32_t accum_pcb_size = 0;
		for(uint32_t i = 0; i < ep_layout->getParameterCount(); i++)
		{
			auto param = ep_layout->getParameterByIndex(i);
			if(param->getCategoryCount() == 0)
				continue; 

			auto category = static_cast<SlangParameterCategory>(param->getCategoryByIndex(0));
			if(category == SLANG_PARAMETER_CATEGORY_PUSH_CONSTANT_BUFFER || category == SLANG_PARAMETER_CATEGORY_UNIFORM)
			{
				auto ptype = param->getTypeLayout();
				std::println("entrypoint {} param {} - {} is PushConstant<{}> size {}", ep, i, param->getName(), ptype->getType()->getName(), ptype->getSize(category));
				accum_pcb_size += static_cast<uint32_t>(ptype->getSize(category));

			}	
		}	

		if(accum_pcb_size)
		{
			if(accum_pcb_size > 64)
				std::println("PushConstant larger than 64 bytes");

			ctx.pconst_size = std::max(ctx.pconst_size, accum_pcb_size);
			if(ctx.ep_types[ep] == 0)
			{
				std::print(" VS");
				ctx.pconst_stages |= penumbra::SHADER_STAGE_VERTEX;
			}
			else if(ctx.ep_types[ep] == 1)
			{
				std::print(" FS");
				ctx.pconst_stages |= penumbra::SHADER_STAGE_FRAGMENT;
			}
			else if(ctx.ep_types[ep] == 2)
			{
				std::print(" CS");
				ctx.pconst_stages |= penumbra::SHADER_STAGE_COMPUTE;
			}
		}
	}
	
	{
	using namespace penumbra;

	std::ofstream out{argv[2], std::ios::binary};
	std::array<uint32_t, 5> code_offsets;

	ShaderFileFormat::Header header;
	header.cbuffer_stages = ctx.cbuffer_stages;
	header.cbuffer_size = ctx.cbuffer_size;
	header.pcb_size = ctx.pconst_size;
	header.pcb_stages = ctx.pconst_stages;
	header.num_stages = ctx.ep_count;
	out.seekp(sizeof(ShaderFileFormat::Header) + sizeof(ShaderFileFormat::ShaderStage) * ctx.ep_count);
	
	for(uint32_t i = 0; i < ctx.ep_count; i++)
	{
		code_offsets[i] = static_cast<uint32_t>(out.tellp());
		out.write(reinterpret_cast<const char*>(ctx.stage_code[i]->getBufferPointer()), ctx.stage_code[i]->getBufferSize());
	}

	out.seekp(sizeof(ShaderFileFormat::Header));
	for(uint32_t i = 0; i < ctx.ep_count; i++)
	{
		ShaderFileFormat::ShaderStage stg;
		if(ctx.ep_types[i] == 0)
			stg.stage = SHADER_STAGE_VERTEX;
		else if(ctx.ep_types[i] == 1)
			stg.stage = SHADER_STAGE_FRAGMENT;
		else if(ctx.ep_types[i] == 2)
			stg.stage = SHADER_STAGE_COMPUTE;

		stg.code_size = ctx.stage_code[i]->getBufferSize();
		stg.code_offset = code_offsets[i];

		out.write(reinterpret_cast<const char*>(&stg), sizeof(ShaderFileFormat::ShaderStage));
	}
	out.seekp(0);
	out.write(reinterpret_cast<const char*>(&header), sizeof(ShaderFileFormat::Header));

	}	
	
	std::println("compiled shader {}", argv[1]);

	return 0;
}
