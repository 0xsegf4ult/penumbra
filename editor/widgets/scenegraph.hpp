#pragma once

#include <widgets/widget.hpp>

#include <penumbra/ecs.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct WorldState;

class ScenegraphView : public Widget
{
public:
	ScenegraphView(WorldState* ws);
	void on_draw() override;
private:
	bool tree_draw(ecs::entity ent, u32& ctr);

	WorldState* world{nullptr};
};

}
