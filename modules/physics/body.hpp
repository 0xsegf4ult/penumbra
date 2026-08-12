#pragma once

#include <penumbra/physics.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct physicsBody
{
	physicsBodyType body_type;
	physicsMotionType motion_type;
	u16 generation{0};

	float mass;
	mat3 inertia;

	physicsShapeID shape;

	u64 userdata;
};

constexpr u32 physics_bodyid_handle(physicsBodyID id)
{
	return id & 0xFFFFFFFF;
};

constexpr u16 physics_bodyid_generation(physicsBodyID id)
{
	return (id >> 32) & 0xFFFF;
}

constexpr physicsBodyID physics_bodyid_new(u32 handle, u16 generation)
{
	return physicsBodyID{handle | (static_cast<u64>(generation) << 32)};
}

physicsBody& physics_body_get(physicsBodyID id);

}
