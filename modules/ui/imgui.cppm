module;

#include <tracy/Tracy.hpp>

export module penumbra.ui:imgui;

import penumbra.core;
import penumbra.math;
import penumbra.gpu;

import imgui;
import std;

using std::uint32_t, std::int32_t, std::uint16_t, std::size_t, std::uint64_t, std::uint8_t, std::memcpy;

namespace penumbra
{

ImGuiContext* imgui_context = nullptr;

struct imgui_backend_penumbra
{
	Window* window;

	ImGuiPlatformImeData ime_data;
	bool ime_dirty{false};

	std::vector<std::function<void()>> hooks;
};

constexpr size_t max_vertices = 65536;
constexpr size_t max_indices = 65536;

struct imgui_renderer_penumbra_gpu
{
	GPUTexture font_texture;
	GPUTextureDescriptor font_texture_view;
	GPUPipeline pso;

	GPUPointer vertex_data;
	GPUPointer index_data;

	int frame_index;
};

void platform_update_ime()
{
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(ImGui::GetIO().BackendPlatformUserData);
	auto* data = &bd->ime_data;

	if(!(data->WantVisible || data->WantTextInput))
		bd->window->stop_text_input();

	if(!bd->ime_dirty)
		return;

	bd->ime_dirty = false;

	if(!bd->window->text_input_active() && (data->WantVisible || data->WantTextInput))
		bd->window->start_text_input();
}

void platform_set_ime_data(ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData* data)
{
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(ImGui::GetIO().BackendPlatformUserData);
	bd->ime_data = *data;
	bd->ime_dirty = true;
	platform_update_ime();

}
export bool imgui_backend_init(Window* window)
{
	imgui_context = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_IsSRGB;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigWindowsResizeFromEdges = true;
	
	imgui_backend_penumbra* bd = new imgui_backend_penumbra();
	io.BackendPlatformUserData = reinterpret_cast<void*>(bd);
	io.BackendPlatformName = "imgui_impl_penumbra";
	
	bd->window = window;

	ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
	platform_io.Platform_SetImeDataFn = platform_set_ime_data;

	window->register_text_event_listener
	([](const char* text)
	{
		ImGui::GetIO().AddInputCharactersUTF8(text);
	});

	window->register_mouse_move_event_listener
	([](float x, float y, float, float)
	{
		ImGui::GetIO().AddMousePosEvent(x, y);
	});

	window->register_mouse_button_event_listener
	([](input_mouse_button button, bool down)
	{
		int btn = -1;

		switch(button)
		{
		case input_mouse_button::left:
			btn = 0;
			break;
		case input_mouse_button::middle:
			btn = 2;
			break;
		case input_mouse_button::right:
			btn = 1;
			break;
		case input_mouse_button::side1:
			btn = 3;
			break;
		case input_mouse_button::side2:
			btn = 4;
			break;
		}

		if(btn >= 0)
			ImGui::GetIO().AddMouseButtonEvent(btn, down);
	});
		
	window->register_mouse_wheel_event_listener
	([](float x, float y)
	{
		ImGui::GetIO().AddMouseWheelEvent(-x, y);
	});

	imgui_renderer_penumbra_gpu* rd = new imgui_renderer_penumbra_gpu();
	io.BackendRendererUserData = reinterpret_cast<void*>(rd);
	io.BackendRendererName = "imgui_renderer_penumbra_gpu";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		
	io.Fonts->AddFontDefault();

	int width = 0;
	int height = 0;
	uint8_t* pixels = nullptr;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	auto font_data = gpu_allocate_memory(width * height * 4ull, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
       	memcpy(gpu_map_memory(font_data), pixels, width * height * 4ull);

	rd->font_texture = gpu_create_texture
	({
		.dim = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1u},
       		.format = GPU_FORMAT_RGBA8_UNORM,
		.usage = GPU_TEXTURE_SAMPLED
	});
	rd->font_texture_view = gpu_texture_view_descriptor(rd->font_texture, {.format = GPU_FORMAT_RGBA8_UNORM});

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, rd->font_texture_view, GPU_STAGE_NONE, GPU_STAGE_TRANSFER, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_copy_to_texture(cmd, font_data, rd->font_texture_view);
	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_FRAGMENT_SHADER);
	auto sig = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	gpu_wait_queue(GPU_QUEUE_GRAPHICS, sig);
	gpu_free_memory(font_data);

	io.Fonts->TexID = static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(&rd->font_texture_view));
	
	rd->vertex_data = gpu_allocate_memory(sizeof(ImDrawVert) * max_vertices * 2, GPU_MEMORY_HOST, GPU_BUFFER_VERTEX);
	rd->index_data = gpu_allocate_memory(sizeof(ImDrawIdx) * max_indices * 2, GPU_MEMORY_HOST, GPU_BUFFER_INDEX);

	auto shader = load_shader("shaders/imgui");
	if(!shader.has_value())
	{
		log::error("imgui_backend: failed to load shader: {}", shader.error());
		return false;
	}

	GPUBlendDesc alpha_blend
	{
		.src_color_factor = GPU_BLEND_FACTOR_SRC_ALPHA,
		.dst_color_factor = GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.src_alpha_factor = GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.dst_alpha_factor = GPU_BLEND_FACTOR_ZERO
	};

	rd->pso = gpu_create_graphics_pipeline(*shader,
	{		
		.color_targets = {GPU_FORMAT_BGRA8_SRGB},
		.blendstate = &alpha_blend
	});

	rd->frame_index = 0;

	return true;
}

export void imgui_backend_shutdown()
{
	ImGuiIO& io = ImGui::GetIO();
	auto* rd = reinterpret_cast<imgui_renderer_penumbra_gpu*>(io.BackendRendererUserData);

	gpu_destroy_pipeline(rd->pso);
	gpu_free_memory(rd->index_data);
	gpu_free_memory(rd->vertex_data);
	gpu_destroy_texture(rd->font_texture);

	io.BackendRendererName = nullptr;
	io.BackendRendererUserData = nullptr;
	delete rd;
	
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(io.BackendPlatformUserData);

	io.BackendPlatformName = nullptr;
	io.BackendPlatformUserData = nullptr;

	delete bd;
	ImGui::DestroyContext(imgui_context);
	imgui_context = nullptr;
}

export void imgui_backend_render(GPUCommandBuffer& cmd, double dt)
{
	ZoneScoped;

	ImGuiIO& io = ImGui::GetIO();
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(io.BackendPlatformUserData);

	auto wdim = bd->window->get_size();

	io.DisplaySize.x = static_cast<float>(wdim.w);
	io.DisplaySize.y = static_cast<float>(wdim.h);
	io.DisplayFramebufferScale.x = 1.0f;
	io.DisplayFramebufferScale.y = 1.0f;

	io.DeltaTime = dt > 0.0 ? static_cast<float>(dt) : (1.0f / 60.0f);

	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

	platform_update_ime();

	{
		ZoneScopedN("user_hooks");
		for(auto& hook : bd->hooks)
			hook();
	}

	ImGui::ShowDemoWindow();

	ImGui::Render();

	auto* rd = reinterpret_cast<imgui_renderer_penumbra_gpu*>(io.BackendRendererUserData);
	auto* draw_data = ImGui::GetDrawData();

	{
		auto* vmem = reinterpret_cast<ImDrawVert*>(gpu_map_memory(rd->vertex_data)) + (rd->frame_index * max_vertices);
		auto* imem = reinterpret_cast<ImDrawIdx*>(gpu_map_memory(rd->index_data)) + (rd->frame_index * max_indices);
		for(const ImDrawList* draw_list : draw_data->CmdLists)
		{
			memcpy(vmem, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(imem, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vmem += draw_list->VtxBuffer.Size;
			imem += draw_list->IdxBuffer.Size;
		}
	}

	gpu_set_pipeline(cmd, rd->pso);

	const ImVec2 clip_off = draw_data->DisplayPos;
	const ImVec2 clip_scale = draw_data->FramebufferScale;

	struct ShaderData
	{
		mat4 proj;
		GPUDevicePointer vertex_data;
		uint32_t textureID;
	} shader_data;
	shader_data.vertex_data = gpu_host_to_device_pointer(rd->vertex_data);
	shader_data.proj = mat4::make_ortho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, 0.0f, 1.0f);
	gpu_bind_index_buffer(cmd, rd->index_data, GPU_INDEX_TYPE_U16);

	uint32_t g_vtx_offset = rd->frame_index * max_vertices;
	uint32_t g_idx_offset = rd->frame_index * max_indices;
	for(const ImDrawList* draw_list : draw_data->CmdLists)
	{
		for(int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
			if(pcmd->UserCallback)
				pcmd->UserCallback(draw_list, pcmd);
			else
			{
				ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
				ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

				if(clip_min.x < 0.0f) { clip_min.x = 0.0f; }
				if(clip_min.y < 0.0f) { clip_min.y = 0.0f; }
				if(clip_max.x > io.DisplaySize.x) { clip_max.x = io.DisplaySize.x; }
				if(clip_max.y > io.DisplaySize.y) { clip_max.y = io.DisplaySize.y; }
				if(clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
					continue;

				uvec4 scissor
				{
					uint32_t(clip_min.x), uint32_t(clip_min.y),
					uint32_t(clip_max.x - clip_min.x),
					uint32_t(clip_max.y - clip_min.y)
				};
				gpu_set_scissor(cmd, scissor);

				if(pcmd->GetTexID())
					shader_data.textureID = reinterpret_cast<GPUTextureDescriptor*>(pcmd->GetTexID())->handle;
				else
					shader_data.textureID = 0;

				gpu_draw_indexed(cmd, &shader_data, pcmd->ElemCount, 1u, pcmd->IdxOffset + g_idx_offset, pcmd->VtxOffset + g_vtx_offset, 0);
			}
		}
		g_vtx_offset += draw_list->VtxBuffer.Size;
		g_idx_offset += draw_list->IdxBuffer.Size;
	}

	rd->frame_index = (rd->frame_index + 1) % 2;
	ImGui::EndFrame();
}	

export void imgui_add_hook(std::function<void()>&& hook)
{
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(ImGui::GetIO().BackendPlatformUserData);
	bd->hooks.push_back(hook);
}

}
