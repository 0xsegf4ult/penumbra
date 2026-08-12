#pragma once

#include <widgets/widget.hpp>
#include <string>
#include <vector>

namespace penumbra
{

class ConsoleWidget : public Widget
{
public:
	ConsoleWidget();

	void on_draw() override;
private:
	std::vector<std::string> items;
	std::vector<std::string> history;
	char input_buffer[256];
};

}
