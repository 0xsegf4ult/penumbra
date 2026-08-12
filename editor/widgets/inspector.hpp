#pragma once

#include <widgets/widget.hpp>
#include <world/state.hpp>

namespace penumbra
{

class Inspector : public Widget
{
public:
	Inspector(WorldState* ws);

	void on_draw() override;
private:
	WorldState* world{nullptr};
};

}
