module penumbra.physics:world;

import :api;

namespace penumbra
{

struct physicsWorld
{
};

static physicsWorld worlds[1];

physicsWorldID physics_create_world(const physicsWorldDesc& desc)
{
	return physicsWorldID{1};
}

void physics_destroy_world(physicsWorldID id)
{
}

void physics_world_simulate(physicsWorldID id, float dt, int substeps)
{

}

}
