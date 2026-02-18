export module penumbra.editor:widget;

import imgui;
import std;

namespace penumbra
{

export class Widget
{
public:
	Widget(std::string_view title, ImGuiWindowFlags flags = 0) : title{title}, wflags{flags} {}
	virtual ~Widget() = default;

	virtual void pre_draw() {}
	virtual void on_draw() {}

	void draw()
	{
		pre_draw();
		
		ImGui::Begin(title.data(), &open, wflags);
		
		on_draw();

		ImGui::End();
	}
protected:
	std::string_view title;
	ImGuiWindowFlags wflags;
	bool open{true};
};

}
