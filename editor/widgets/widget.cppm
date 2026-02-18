export module penumbra.editor:widget;

import penumbra.math;
import imgui;
import std;

namespace penumbra
{

export class Widget
{
public:
	Widget(std::string_view title, ImGuiWindowFlags flags = 0) : title{title}, wflags{flags} {}
	virtual ~Widget() = default;

	virtual void configure() {}
	virtual void on_draw() {}
	
	uvec2 get_size() const
	{
		return size;
	}

	void draw()
	{
		configure();
		
		ImGui::Begin(title.data(), &open, wflags);
		
		on_draw();

		size.x = ImGui::GetWindowWidth();
		size.y = ImGui::GetWindowHeight();

		ImGui::End();
	}
protected:
	std::string_view title;
	ImGuiWindowFlags wflags;
	uvec2 size{0u};
	bool open{true};
};

}
