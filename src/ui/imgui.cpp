#include <penumbra/ui.hpp>
#include <penumbra/input.hpp>
#include <penumbra/input_keys.hpp>
#include <penumbra/window.hpp>
#include <penumbra/math/matrix.hpp>
#include <penumbra/math/vector.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/shader.hpp>
#include <penumbra/types.hpp>

#include <tracy/Tracy.hpp>

#include <array>
#include <cstring>
#include <functional>
#include <vector>

using std::memcpy;

namespace penumbra
{

static ImGuiContext* imgui_context = nullptr;

struct imgui_backend_penumbra
{
	window_t window;

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
	std::array<GPUPointer, 2> cbuf_matrix;

	int frame_index;
};

static void platform_update_ime()
{
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(ImGui::GetIO().BackendPlatformUserData);
	auto* data = &bd->ime_data;
	
	if(!(data->WantVisible || data->WantTextInput))
		input_stop_text_input();

	if(!bd->ime_dirty)
		return;

	bd->ime_dirty = false;

	if(!input_text_input_active() && (data->WantVisible || data->WantTextInput))
		input_start_text_input();
}

static void platform_set_ime_data(ImGuiContext*, ImGuiViewport*, ImGuiPlatformImeData* data)
{
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(ImGui::GetIO().BackendPlatformUserData);
	bd->ime_data = *data;
	bd->ime_dirty = true;
	platform_update_ime();
}

constexpr static ImGuiKey key_to_imgui_key(keycode_t key);

static void imgui_handle_key_event(keycode_t key, bool down)
{
	auto& io = ImGui::GetIO();
	switch(key)
	{
	case KEY_LCONTROL:
	case KEY_RCONTROL:
		io.AddKeyEvent(ImGuiMod_Ctrl, down);
		break;
	case KEY_LSHIFT:
	case KEY_RSHIFT:
		io.AddKeyEvent(ImGuiMod_Shift, down);
		break;
	case KEY_LALT:
	case KEY_RALT:
		io.AddKeyEvent(ImGuiMod_Alt, down);
		break;
	case KEY_LGUI:
	case KEY_RGUI:
		io.AddKeyEvent(ImGuiMod_Super, down);
		break;
	default:
		break;
	}

	io.AddKeyEvent(key_to_imgui_key(key), down);
}

static void imgui_handle_input_event(const input_event_t& event)
{
	auto& io = ImGui::GetIO();

	switch(event.type)
	{
	case INPUT_EVENT_KEY_DOWN:
	case INPUT_EVENT_KEY_UP:
		imgui_handle_key_event(event.key.scancode, event.type == INPUT_EVENT_KEY_DOWN);
		break;
	case INPUT_EVENT_TEXT_INPUT:
		io.AddInputCharactersUTF8(event.text.data);
		break;
	case INPUT_EVENT_MOUSE_MOTION:
		io.AddMousePosEvent(event.mouse_motion.pos.x, event.mouse_motion.pos.y);
		break;
	case INPUT_EVENT_MOUSE_BUTTON_DOWN:
	case INPUT_EVENT_MOUSE_BUTTON_UP:
		io.AddMouseButtonEvent(event.mouse_button.button - MOUSE_LEFT, event.type == INPUT_EVENT_MOUSE_BUTTON_DOWN);
		break;
	case INPUT_EVENT_MOUSE_WHEEL:
		io.AddMouseWheelEvent(-event.mouse_wheel.delta.x, event.mouse_wheel.delta.y);
		break;
	default:
		break;
	}
}

bool imgui_backend_init(window_t window)
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

	input_register_listener(imgui_handle_input_event);

	imgui_renderer_penumbra_gpu* rd = new imgui_renderer_penumbra_gpu();
	io.BackendRendererUserData = reinterpret_cast<void*>(rd);
	io.BackendRendererName = "imgui_renderer_penumbra_gpu";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		
	io.Fonts->AddFontDefault();

	int width = 0;
	int height = 0;
	u8* pixels = nullptr;

	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	auto font_data = gpu_allocate_memory(width * height * 4ull, GPU_MEMORY_HOST, GPU_BUFFER_UPLOAD);
       	memcpy(gpu_map_memory(font_data), pixels, width * height * 4ull);

	rd->font_texture = gpu_create_texture
	({
		.dim = {static_cast<u32>(width), static_cast<u32>(height), 1u},
       		.format = GPU_FORMAT_RGBA8_UNORM,
		.usage = GPU_TEXTURE_SAMPLED
	});
	rd->font_texture_view = gpu_texture_view_descriptor(rd->font_texture, {.format = GPU_FORMAT_RGBA8_UNORM});

	auto cmd = gpu_record_commands(GPU_QUEUE_GRAPHICS);
	gpu_texture_layout_transition(cmd, rd->font_texture, GPU_STAGE_NONE, GPU_STAGE_TRANSFER, GPU_TEXTURE_LAYOUT_UNDEFINED, GPU_TEXTURE_LAYOUT_GENERAL);
	gpu_copy_to_texture(cmd, font_data, rd->font_texture);
	gpu_barrier(cmd, GPU_STAGE_TRANSFER, GPU_STAGE_FRAGMENT_SHADER);
	auto sig = gpu_submit(GPU_QUEUE_GRAPHICS, cmd);
	gpu_wait_queue(GPU_QUEUE_GRAPHICS, sig);
	gpu_free_memory(font_data);

	io.Fonts->TexID = static_cast<ImTextureID>(reinterpret_cast<intptr_t>(&rd->font_texture_view));
	
	rd->vertex_data = gpu_allocate_memory(sizeof(ImDrawVert) * max_vertices * 2, GPU_MEMORY_HOST, GPU_BUFFER_VERTEX);
	rd->index_data = gpu_allocate_memory(sizeof(ImDrawIdx) * max_indices * 2, GPU_MEMORY_HOST, GPU_BUFFER_INDEX);

	for(int i = 0; i < 2; i++)
		rd->cbuf_matrix[i] = gpu_allocate_memory(sizeof(mat4), GPU_MEMORY_HOST, GPU_BUFFER_UNIFORM);

	GPUBlendDesc alpha_blend
	{
		.src_color_factor = GPU_BLEND_FACTOR_SRC_ALPHA,
		.dst_color_factor = GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.src_alpha_factor = GPU_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.dst_alpha_factor = GPU_BLEND_FACTOR_ZERO
	};

	rd->pso = gpu_create_graphics_pipeline(load_shader("shaders/imgui"),
	{		
		.color_targets = {GPU_FORMAT_BGRA8_SRGB},
		.blendstate = &alpha_blend
	});

	rd->frame_index = 0;

	return true;
}

void imgui_backend_shutdown()
{
	ImGuiIO& io = ImGui::GetIO();
	auto* rd = reinterpret_cast<imgui_renderer_penumbra_gpu*>(io.BackendRendererUserData);

	gpu_destroy_pipeline(rd->pso);
	for(int i = 0; i < 2; i++)
		gpu_free_memory(rd->cbuf_matrix[i]);

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

void imgui_backend_render(GPUCommandBuffer& cmd, double dt)
{
	ZoneScoped;

	ImGuiIO& io = ImGui::GetIO();
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(io.BackendPlatformUserData);

	auto wdim = wm_get_size(bd->window);

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
		GPUDevicePointer vertex_data;
		u32 textureID;
	} shader_data;
	shader_data.vertex_data = gpu_host_to_device_pointer(rd->vertex_data);
	
	auto proj = mat4::make_ortho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, 0.0f, 1.0f);
	memcpy(gpu_map_memory(rd->cbuf_matrix[rd->frame_index]), &proj, sizeof(mat4));
	gpu_write_cbuffer_descriptor(cmd, rd->cbuf_matrix[rd->frame_index]);
	
	gpu_bind_index_buffer(cmd, rd->index_data, GPU_INDEX_TYPE_U16);

	u32 g_vtx_offset = rd->frame_index * max_vertices;
	u32 g_idx_offset = rd->frame_index * max_indices;
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
					u32(clip_min.x), u32(clip_min.y),
					u32(clip_max.x - clip_min.x),
					u32(clip_max.y - clip_min.y)
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

void imgui_add_hook(std::function<void()>&& hook)
{
	auto* bd = reinterpret_cast<imgui_backend_penumbra*>(ImGui::GetIO().BackendPlatformUserData);
	bd->hooks.push_back(hook);
}

static constexpr ImGuiKey key_to_imgui_key(keycode_t key)
{
	switch(key)
	{
	case KEY_KP0: return ImGuiKey_Keypad0;
	case KEY_KP1: return ImGuiKey_Keypad1;
	case KEY_KP2: return ImGuiKey_Keypad2;
	case KEY_KP3: return ImGuiKey_Keypad3;
	case KEY_KP4: return ImGuiKey_Keypad4;
	case KEY_KP5: return ImGuiKey_Keypad5;
	case KEY_KP6: return ImGuiKey_Keypad6;
	case KEY_KP7: return ImGuiKey_Keypad7;
	case KEY_KP8: return ImGuiKey_Keypad8;
	case KEY_KP9: return ImGuiKey_Keypad9;
	case KEY_KP_PERIOD: return ImGuiKey_KeypadDecimal;
	case KEY_KP_DIVIDE: return ImGuiKey_KeypadDivide;
	case KEY_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
	case KEY_KP_MINUS: return ImGuiKey_KeypadSubtract;
	case KEY_KP_PLUS: return ImGuiKey_KeypadAdd;
	case KEY_KP_ENTER: return ImGuiKey_KeypadEnter;
	case KEY_TAB: return ImGuiKey_Tab;
	case KEY_LEFT: return ImGuiKey_LeftArrow;
	case KEY_RIGHT: return ImGuiKey_RightArrow;
	case KEY_UP: return ImGuiKey_UpArrow;
	case KEY_DOWN: return ImGuiKey_DownArrow;
	case KEY_PAGEUP: return ImGuiKey_PageUp;
	case KEY_PAGEDOWN: return ImGuiKey_PageDown;
	case KEY_HOME: return ImGuiKey_Home;
	case KEY_END: return ImGuiKey_End;
	case KEY_INSERT: return ImGuiKey_Insert;
	case KEY_DELETE: return ImGuiKey_Delete;
	case KEY_BACKSPACE: return ImGuiKey_Backspace;
	case KEY_SPACE: return ImGuiKey_Space;
	case KEY_ENTER: return ImGuiKey_Enter;
	case KEY_ESCAPE: return ImGuiKey_Escape;
	case KEY_CAPSLOCK: return ImGuiKey_CapsLock;
	case KEY_SCROLLLOCK: return ImGuiKey_ScrollLock;
	case KEY_NUMLOCK: return ImGuiKey_NumLock;
	case KEY_BREAK: return ImGuiKey_Pause;
	case KEY_LCONTROL: return ImGuiKey_LeftCtrl;
	case KEY_LSHIFT: return ImGuiKey_LeftShift;
	case KEY_LALT: return ImGuiKey_LeftAlt;
	case KEY_LGUI: return ImGuiKey_LeftSuper;
	case KEY_RCONTROL: return ImGuiKey_RightCtrl;
	case KEY_RSHIFT: return ImGuiKey_RightShift;
	case KEY_RALT: return ImGuiKey_RightAlt;
	case KEY_RGUI: return ImGuiKey_RightSuper;
	case KEY_APP: return ImGuiKey_Menu;
	case KEY_0: return ImGuiKey_0;
	case KEY_1: return ImGuiKey_1;
	case KEY_2: return ImGuiKey_2;
	case KEY_3: return ImGuiKey_3;
	case KEY_4: return ImGuiKey_4;
	case KEY_5: return ImGuiKey_5;
	case KEY_6: return ImGuiKey_6;
	case KEY_7: return ImGuiKey_7;
	case KEY_8: return ImGuiKey_8;
	case KEY_9: return ImGuiKey_9;
	case KEY_A: return ImGuiKey_A;
	case KEY_B: return ImGuiKey_B;
	case KEY_C: return ImGuiKey_C;
	case KEY_D: return ImGuiKey_D;
	case KEY_E: return ImGuiKey_E;
	case KEY_F: return ImGuiKey_F;
	case KEY_G: return ImGuiKey_G;
	case KEY_H: return ImGuiKey_H;
	case KEY_I: return ImGuiKey_I;
	case KEY_J: return ImGuiKey_J;
	case KEY_K: return ImGuiKey_K;
	case KEY_L: return ImGuiKey_L;
	case KEY_M: return ImGuiKey_M;
	case KEY_N: return ImGuiKey_N;
	case KEY_O: return ImGuiKey_O;
	case KEY_P: return ImGuiKey_P;
	case KEY_Q: return ImGuiKey_Q;
	case KEY_R: return ImGuiKey_R;
	case KEY_S: return ImGuiKey_S;
	case KEY_T: return ImGuiKey_T;
	case KEY_U: return ImGuiKey_U;
	case KEY_V: return ImGuiKey_V;
	case KEY_W: return ImGuiKey_W;
	case KEY_X: return ImGuiKey_X;
	case KEY_Y: return ImGuiKey_Y;
	case KEY_Z: return ImGuiKey_Z;
	case KEY_F1: return ImGuiKey_F1;
	case KEY_F2: return ImGuiKey_F2;
	case KEY_F3: return ImGuiKey_F3;
	case KEY_F4: return ImGuiKey_F4;
	case KEY_F5: return ImGuiKey_F5;
	case KEY_F6: return ImGuiKey_F6;
	case KEY_F7: return ImGuiKey_F7;
	case KEY_F8: return ImGuiKey_F8;
	case KEY_F9: return ImGuiKey_F9;
	case KEY_F10: return ImGuiKey_F10;
	case KEY_F11: return ImGuiKey_F11;
	case KEY_F12: return ImGuiKey_F12;
	case KEY_TILDE: return ImGuiKey_GraveAccent;
	case KEY_MINUS: return ImGuiKey_Minus;
	case KEY_EQUAL: return ImGuiKey_Equal;
	case KEY_LBRACKET: return ImGuiKey_LeftBracket;
	case KEY_RBRACKET: return ImGuiKey_RightBracket;
	case KEY_BACKSLASH: return ImGuiKey_Backslash;
	case KEY_SEMICOLON: return ImGuiKey_Semicolon;
	case KEY_APOSTROPHE: return ImGuiKey_Apostrophe;
	case KEY_COMMA: return ImGuiKey_Comma;
	case KEY_PERIOD: return ImGuiKey_Period;
	case KEY_SLASH: return ImGuiKey_Slash;
	default: return ImGuiKey_None;
	}
}

}
