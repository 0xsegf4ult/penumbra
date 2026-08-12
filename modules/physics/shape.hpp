#pragma once

#include <penumbra/physics.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct physicsShape
{
	u32 id;
	u32 body;
	u16 generation;
	physicsShapeType type;

	union
	{
		physicsSphere sphere;
		physicsCapsule capsule;
		const physicsHull* hull{nullptr};
	};
};

constexpr u32 physics_shapeid_handle(physicsShapeID id)
{
	return id & 0xFFFFFFFF;
}

constexpr u16 physics_shapeid_generation(physicsShapeID id)
{
	return (id >> 32) & 0xFFFF;
}

constexpr physicsShapeID physics_shapeid_new(u32 handle, u16 generation)
{
	return physicsShapeID{handle | (static_cast<u64>(generation) << 32)};
}

physicsShape& physics_shape_get(physicsShapeID shape);

}

