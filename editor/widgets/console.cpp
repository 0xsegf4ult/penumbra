#include <widgets/console.hpp>
#include <widgets/widget.hpp>

#include <penumbra/ui.hpp>
#include <penumbra/types.hpp>

#include <string>
#include <vector>

namespace penumbra
{

ConsoleWidget::ConsoleWidget() : Widget("Console") 
{
	set_open(false);
	input_buffer[0] = '\0';
}

void ConsoleWidget::on_draw()
{
	const float footer_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();

	if(ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_reserve), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_HorizontalScrollbar))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
		
		for(auto& item : items)
			ImGui::TextUnformatted(item.c_str());

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();
	ImGui::Separator();

	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;

	if(ImGui::InputText("Input", input_buffer, 256, input_text_flags, [](ImGuiInputTextCallbackData* data)
	{
		switch(data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
			break;
		case ImGuiInputTextFlags_CallbackHistory:
			break;
		}

		return 0;
	}))
	{
		auto cmd = std::string(input_buffer);
		items.push_back("# " + cmd);
		history.push_back(cmd);
		input_buffer[0] = '\0';
		reclaim_focus = true;
	}

	ImGui::SetItemDefaultFocus();
	if(reclaim_focus)
		ImGui::SetKeyboardFocusHere(-1);
}

}
