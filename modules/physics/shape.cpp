#include <physics/body.hpp>
#include <physics/shape.hpp>
#include <physics/world.hpp>
#include <penumbra/physics.hpp>
#include <penumbra/types.hpp>
#include <vector>

namespace penumbra
{

static physicsShape& physics_shape_new(physicsBodyID body, const physicsShapeDesc& desc)
{
	auto& world = physics_world_get();	
	u32 shapeid;

	if(world.shape_id_freelist.empty())
	{
		shapeid = world.next_shape_id++;
	}
	else
	{
		shapeid = world.shape_id_freelist.back();
		world.shape_id_freelist.pop_back();
	}

	if(shapeid >= world.shapes.size())
		world.shapes.resize(shapeid);

	auto& shape = world.shapes[shapeid - 1];
	shape.id = shapeid;
	shape.generation++;

	return shape;	
}

static void physics_shape_add_to_body(physicsBodyID body_id, physicsShapeID shape_id, physicsShape& shape)
{
	auto& body = physics_body_get(body_id);
	shape.body = physics_bodyid_handle(body_id);
	body.shape = shape_id;
}

physicsShape& physics_shape_get(physicsShapeID id)
{
	u32 handle = physics_shapeid_handle(id);
	u16 generation = physics_shapeid_generation(id);

	auto& world = physics_world_get();
	assert(handle);
	auto& shape = world.shapes[handle - 1];
	assert(generation == shape.generation);
	return shape;
}

physicsShapeID physics_create_sphere(physicsBodyID body, const physicsShapeDesc& desc, const physicsSphere& sphere)
{
	auto& shape = physics_shape_new(body, desc);
	shape.type = PHYSICS_SHAPE_SPHERE;
	shape.sphere = sphere;
	auto id = physics_shapeid_new(shape.id, shape.generation);
	physics_shape_add_to_body(body, id, shape);
	return id;
}

physicsShapeID physics_create_capsule(physicsBodyID body, const physicsShapeDesc& desc, const physicsCapsule& capsule)
{
	auto& shape = physics_shape_new(body, desc);
	shape.type = PHYSICS_SHAPE_CAPSULE;
	shape.capsule = capsule;
	auto id = physics_shapeid_new(shape.id, shape.generation);
	physics_shape_add_to_body(body, id, shape);
	return id;
}

physicsShapeID physics_create_hull(physicsBodyID body, const physicsShapeDesc& desc, const physicsHull& hull)
{
	auto& shape = physics_shape_new(body, desc);
	shape.type = PHYSICS_SHAPE_HULL;
	shape.hull = &hull;
	auto id = physics_shapeid_new(shape.id, shape.generation);
	physics_shape_add_to_body(body, id, shape);
	return id;
}

}
