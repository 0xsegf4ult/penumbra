#pragma once

#include <penumbra/math/plane.hpp>
#include <penumbra/math/transform.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

using physicsBodyID = u64;
using physicsShapeID = u64;

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

enum physicsShapeType : u8
{
	PHYSICS_SHAPE_SPHERE,
	PHYSICS_SHAPE_CAPSULE,
	PHYSICS_SHAPE_HULL,
	PHYSICS_SHAPE_MESH
};

struct physicsWorldDesc
{
	vec3 gravity{0.0f, -9.81f, 0.0f};
};

struct physicsBodyDesc
{
	Transform initial_transform{};
	physicsBodyType body_type{PHYSICS_BODY_DYNAMIC};
	physicsMotionType motion_type{PHYSICS_MOTION_DISCRETE};
	u64 userdata{0u};
};

struct physicsShapeDesc
{
	float density{1.0f};
};

struct physicsSphere
{
	float radius{1.0f};
};

struct physicsCapsule
{
	float radius{1.0f};
	float height{1.0f};
};

struct physicsHull
{
	u32 size_bytes;
	u32 vertex_count;
	u32 edge_count;
	u32 face_count;
};

struct physicsHullHalfEdge
{
};

struct physicsBoxHull
{
	physicsHull hull;
	u8 vertices[8];
	vec3 points[8];
	physicsHullHalfEdge edges[24];
	Plane planes[6];
	u8 faces[6];
};

void physics_create_world(const physicsWorldDesc& desc);
void physics_destroy_world();
void physics_world_simulate(float dt, int substeps);

physicsBodyID physics_create_body(const physicsBodyDesc& desc);
void physics_destroy_body(physicsBodyID body);

physicsShapeID physics_create_sphere(physicsBodyID body, const physicsShapeDesc& desc, const physicsSphere& sphere);
physicsShapeID physics_create_capsule(physicsBodyID body, const physicsShapeDesc& desc, const physicsCapsule& capsule);
physicsShapeID physics_create_hull(physicsBodyID body, const physicsShapeDesc& desc, const physicsHull& hull);

physicsBoxHull physics_make_box_hull(vec3 half_size);

}
