#pragma once

#include <penumbra/math/aabb.hpp>
#include <penumbra/math/plane.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/types.hpp>
#include <box3d/box3d.h>
#include <limits>

namespace penumbra
{

using physicsBody = b3BodyId;
using physicsShape = b3ShapeId;
using physicsCastCallback = b3CastResultFcn;
using physicsPlaneCallback = b3PlaneResultFcn;
using physicsShapeProxy = b3ShapeProxy;
using physicsCollisionPlane = b3CollisionPlane;
using physicsPlaneResult = b3PlaneResult;
using physicsPlaneSolverResult = b3PlaneSolverResult;

enum physicsBodyType : u8
{
	PHYSICS_BODY_STATIC,
	PHYSICS_BODY_KINEMATIC,
	PHYSICS_BODY_DYNAMIC
};

enum physicsMotionType : u8
{
	PHYSICS_MOTION_DISCRETE,
	PHYSICS_MOTION_CCD
};

struct physicsBodyDesc
{
	physicsBodyType type{PHYSICS_BODY_DYNAMIC};
	physicsMotionType motion{PHYSICS_MOTION_DISCRETE};
	vec3 position{0.0f};
	Quaternion rotation{};
};

struct physicsShapeDesc
{
	u64 userdata{0u};
};

struct physicsSphere
{
	vec3 center;
	float radius;
};

struct physicsCapsule
{
	vec3 center1;
	vec3 center2;
	float radius;
};

struct physicsQueryFilter
{
	u64 category{std::numeric_limits<u64>::max()};
	u64 mask{std::numeric_limits<u64>::max()};
};

struct physicsRayResult
{
	physicsShape shape;
	vec3 point;
	vec3 normal;
	float fraction;
	bool hit;
};

struct physicsMoveTrace
{
	float fraction;
	vec3 normal;
};

void physics_init();
void physics_shutdown();

void physics_world_simulate(float dt, int substeps);

physicsBody physics_create_body(const physicsBodyDesc& desc);
physicsShape physics_create_sphere(physicsBody body, const physicsShapeDesc& desc, const physicsSphere& sphere);
physicsShape physics_create_capsule(physicsBody body, const physicsShapeDesc& desc, const physicsCapsule& capsule);
physicsShape physics_create_box(physicsBody body, const physicsShapeDesc& desc, vec3 half_sizes);
void physics_world_cast_ray(vec3 origin, vec3 translation, physicsQueryFilter filter, physicsCastCallback callback, void* callback_context);
physicsRayResult physics_world_cast_ray_closest(vec3 origin, vec3 translation, physicsQueryFilter filter);
void physics_world_cast_shape(vec3 origin, const physicsShapeProxy& proxy, vec3 translation, physicsQueryFilter filter, physicsCastCallback callback, void* callback_context);
float physics_world_cast_mover(vec3 origin, const physicsCapsule& capsule, vec3 translation, physicsQueryFilter filter);
void physics_world_collide_mover(vec3 origin, const physicsCapsule& capsule, physicsQueryFilter filter, physicsPlaneCallback callback, void* context);
physicsPlaneSolverResult physics_solve_planes(vec3 target_delta, physicsCollisionPlane* planes, int count);
vec3 physics_clip_vector(vec3 vector, const physicsCollisionPlane* planes, int count);

void physics_body_set_target_transform(physicsBody body, vec3 position, Quaternion rotation, float timestep, bool wake);

u64 physics_shape_get_userdata(physicsShape shape);

}
