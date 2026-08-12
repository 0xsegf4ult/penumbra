#include <physics/body.hpp>
#include <physics/shape.hpp>
#include <physics/world.hpp>
#include <penumbra/physics.hpp>
#include <penumbra/types.hpp>
#include <cassert>

namespace penumbra
{

static physicsWorld* world = nullptr;

void physics_create_world(const physicsWorldDesc& desc)
{
	world = new physicsWorld();
}

void physics_destroy_world()
{
	delete world;
}

void physics_world_simulate(float dt, int substeps)
{
}

physicsWorld& physics_world_get()
{
	return *world;
}

}
