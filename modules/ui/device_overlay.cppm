export module penumbra.ui:device_overlay;

import penumbra.core;
import penumbra.math;
import penumbra.gpu;

import imgui;
import std;

export namespace penumbra::ui
{

void draw_device_overlay(uvec2 root = {0u, 0u})
{
	const float fps = ImGui::GetIO().Framerate;
	static bool p_open = true;
	ImGui::SetNextWindowPos(ImVec2(static_cast<float>(root.x), static_cast<float>(root.y)), ImGuiCond_Always);
	const ImGuiWindowFlags wflags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs;
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1.0f, 1.0f));
	ImGui::Begin("device_overlay", &p_open, wflags);

	ImGui::Text("penumbra git-%s", config::git_hash);
	
	ImColor fps_color = ImColor(20, 220, 20, 255);
	if(fps < 45.0f)
		fps_color = ImColor(220, 20, 20, 255);
	else if(fps < 59.0f)
		fps_color = ImColor(180, 220, 20, 255);

	ImGui::TextColored(fps_color, "%.0f FPS (%.2f mspf)", fps, 1000.0f / fps);
	
	ImGui::End();
	ImGui::PopStyleVar(2);
}

}
