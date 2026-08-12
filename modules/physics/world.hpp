#pragma once

#include <penumbra/physics.hpp>
#include <penumbra/types.hpp>
#include <vector>

struct physicsBody;
struct physicsShape;

namespace penumbra
{

struct physicsWorld
{
	std::vector<physicsBody> bodies;
	std::vector<u32> body_id_freelist;
	u32 next_body_id = 1u;

	std::vector<physicsShape> shapes;
	std::vector<u32> shape_id_freelist;
	u32 next_shape_id = 1u;
};

physicsWorld& physics_world_get();

}
