#include <physics/body.hpp>
#include <physics/world.hpp>
#include <penumbra/physics.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/types.hpp>
#include <cassert>
#include <vector>

namespace penumbra
{

physicsBodyID physics_create_body(const physicsBodyDesc& desc)
{
	auto& world = physics_world_get();
	u32 bodyid;

	if(world.body_id_freelist.empty())
	{
		bodyid = world.next_body_id++;
	}
	else
	{
		bodyid = world.body_id_freelist.back();
		world.body_id_freelist.pop_back();
	}

	if(bodyid >= world.bodies.size())
		world.bodies.resize(bodyid);

	auto& body = world.bodies[bodyid - 1];
	body.generation++;
	body.body_type = desc.body_type;
	body.motion_type = desc.motion_type;
	body.mass = 0.0f;
	body.inertia = mat3::identity();
	body.userdata = desc.userdata;

	return physics_bodyid_new(bodyid, body.generation);
}

void physics_destroy_body(physicsBodyID id)
{
	auto& body = physics_body_get(id);
	u32 handle = physics_bodyid_handle(id);
	auto& world = physics_world_get();

	world.body_id_freelist.push_back(handle);
}

physicsBody& physics_body_get(physicsBodyID id)
{
	u32 handle = physics_bodyid_handle(id);
	u16 generation = physics_bodyid_generation(id);

	assert(handle);
	auto& world = physics_world_get();
	assert(handle <= world.bodies.size());
	auto& body = world.bodies[handle - 1];
	assert(generation == body.generation);

	return body;
}

}
