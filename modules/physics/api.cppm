export module penumbra.physics:api;

import penumbra.core;
import penumbra.math;

import std;

export import :shape;

using std::uint32_t, std::uint16_t;

export namespace penumbra
{

using physicsWorldID = strongly_typed<uint32_t, struct _pw_id_tag>;

struct physicsWorldDesc
{
	vec3 gravity{0.0f, -9.81f, 0.0f};
};

physicsWorldID physics_create_world(const physicsWorldDesc& desc);
void physics_destroy_world(physicsWorldID id);
void physics_world_simulate(physicsWorldID id, float dt, int substeps);

physicsShapeRef physics_create_sphere(const physicsSphereDesc& desc);
physicsShapeRef physics_create_capsule(const physicsCapsuleDesc& desc);
physicsShapeRef physics_create_box(const physicsBoxDesc& desc);

enum physicsBodyType
{
	PHYSICS_BODY_STATIC,
	PHYSICS_BODY_KINEMATIC,
	PHYSICS_BODY_DYNAMIC
};

enum physicsMotionType
{
	PHYSICS_MOTION_DISCRETE,
	PHYSICS_MOTION_CCD
};

struct physicsBodyDesc
{
	Transform initial_transform;
	const physicsShapeRef& shape;
	physicsBodyType body_type{PHYSICS_BODY_DYNAMIC};
	physicsMotionType motion_type{PHYSICS_MOTION_DISCRETE};
};

struct physicsBodyID
{
	uint32_t handle;
	uint16_t world;
};

physicsBodyID physics_create_body(physicsWorldID world, const physicsBodyDesc& desc);

struct physicsDistanceInput
{
	const physicsShape& shape_a;
	const physicsShape& shape_b;
	Transform transform_a;
	Transform transform_b;
};

struct physicsDistanceOutput
{
	vec3 normal;
	float distance;
	vec3 point_a;
	vec3 point_b;
};

physicsDistanceOutput physics_shape_distance(const physicsDistanceInput& input);

struct physicsShapeCastInput
{
	const physicsShape& shape_a;
	const physicsShape& shape_b;
	Transform transform_a;
	Transform transform_b;
	vec3 direction;
	float fraction = 1.0f;
};

struct physicsShapeCastOutput
{
	vec3 normal;
	float fraction;
	vec3 point;
	bool hit;
};

physicsShapeCastOutput physics_shape_cast(const physicsShapeCastInput& input);


}
