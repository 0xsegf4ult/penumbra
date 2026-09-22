#include <penumbra/ui.hpp>
#include <penumbra/gpu.hpp>
#include <penumbra/renderer.hpp>
#include <penumbra/config.hpp>
#include <penumbra/cvar.hpp>
#include <penumbra/types.hpp>

namespace penumbra::ui
{

static const char* gpu_pass_names[RENDER_GPU_PASS_COUNT] =
{
	"frame", "update", "cull", "prepass", "shadow", "resolve", "forward", "bloom", "compose"
};

static cvar_t perf
{
	.name = "perf",
	.type = CVAR_TYPE_INT,
	.int_defv = 2
};

void device_overlay_init()
{
	cvar_register(&perf);
}

void draw_device_overlay(uvec2 root)
{
	if(!perf.int_v)
		return;

	const float fps = ImGui::GetIO().Framerate;
	static bool p_open = true;
	ImGui::SetNextWindowPos(ImVec2(static_cast<float>(root.x), static_cast<float>(root.y)), ImGuiCond_Always);
	const ImGuiWindowFlags wflags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
	ImGui::Begin("device_overlay", &p_open, wflags);

	ImGui::Text("penumbra git-%s", config::git_hash);

	const auto& gpu_props = gpu_get_properties();
	ImGui::Text("%s", gpu_props.device_name.c_str());

	ImColor fps_color = ImColor(20, 220, 20, 255);
	if(fps < 45.0f)
		fps_color = ImColor(220, 20, 20, 255);
	else if(fps < 59.0f)
		fps_color = ImColor(180, 220, 20, 255);

	ImGui::TextColored(fps_color, "%.0f FPS (%.2f mspf)", fps, 1000.0f / fps);

	if(perf.int_v > 1)
	{
		const auto& timings = renderer_gpu_timings();
		if(timings.frame)
		{
			for(u32 i = 0; i < RENDER_GPU_PASS_COUNT; i++)
			{
				const u64 ns = timings.ns[i][1] - timings.ns[i][0];
				ImGui::Text("%-7s %8.3f ms", gpu_pass_names[i], double(ns) / 1e6);
			}
		}
	}
	
	ImGui::End();
	ImGui::PopStyleVar(2);
}

}
