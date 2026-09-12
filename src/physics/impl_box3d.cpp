#include <penumbra/physics.hpp>
#include <penumbra/log.hpp>
#include <box3d/box3d.h>

#include <tracy/Tracy.hpp>

namespace penumbra
{

static b3WorldId world;

static void impl_box3d_log(const char* message)
{
	log::info("physics_box3d: {}", message);
}

void physics_init()
{
	b3SetLogFcn(impl_box3d_log);
	
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity = (b3Vec3){0.0f, -9.81f, 0.0f};

	world = b3CreateWorld(&worldDef);

	b3Version ver = b3GetVersion();
	log::info("physics: initialized backend Box3D v{}.{}.{}", ver.major, ver.minor, ver.revision);
}

void physics_shutdown()
{
	b3DestroyWorld(world);
}

void physics_world_simulate(float dt, int substeps)
{
	ZoneScoped;
	b3World_Step(world, dt, substeps);
}

physicsBody physics_create_body(const physicsBodyDesc& desc)
{
	b3BodyDef def = b3DefaultBodyDef();
	def.position = (b3Vec3){desc.position.x, desc.position.y, desc.position.z};

	switch(desc.type)
	{
	case PHYSICS_BODY_STATIC:
		def.type = b3_staticBody;
		break;
	case PHYSICS_BODY_KINEMATIC:
		def.type = b3_kinematicBody;
		break;
	case PHYSICS_BODY_DYNAMIC:
		def.type = b3_dynamicBody;
		break;
	}

	return b3CreateBody(world, &def);
}

void physics_destroy_body(physicsBody body)
{
	b3DestroyBody(body);
}

bool physics_body_is_valid(physicsBody body)
{
	return b3Body_IsValid(body);
}

static b3ShapeDef parse_shapedef(const physicsShapeDesc& desc)
{
	auto def = b3DefaultShapeDef();
	def.userData = (void*)desc.userdata;
	return def;
}

physicsShape physics_create_sphere(physicsBody body, const physicsShapeDesc& desc, const physicsSphere& sphere)
{
	auto def = parse_shapedef(desc);
	return b3CreateSphereShape(body, &def, reinterpret_cast<const b3Sphere*>(&sphere));
}

physicsShape physics_create_capsule(physicsBody body, const physicsShapeDesc& desc, const physicsCapsule& capsule)
{
	auto def = parse_shapedef(desc);
	return b3CreateCapsuleShape(body, &def, reinterpret_cast<const b3Capsule*>(&capsule));
}

physicsShape physics_create_box(physicsBody body, const physicsShapeDesc& desc, vec3 half_sizes)
{
	b3BoxHull box = b3MakeBoxHull(half_sizes.x, half_sizes.y, half_sizes.z);

	auto def = parse_shapedef(desc);
	return b3CreateHullShape(body, &def, &box.base);
}

void physics_world_cast_ray(vec3 origin, vec3 translation, physicsQueryFilter filter, physicsCastCallback callback, void* callback_context)
{
	b3Pos b_origin = b3ToPos((b3Vec3){origin.x, origin.y, origin.z});
	b3Vec3 b_translation = (b3Vec3){translation.x, translation.y, translation.z};
	b3QueryFilter b_filter = b3DefaultQueryFilter();
	b_filter.categoryBits = filter.category;
	b_filter.maskBits = filter.mask;

	b3World_CastRay(world, b_origin, b_translation, b_filter, callback, callback_context);
}

physicsRayResult physics_world_cast_ray_closest(vec3 origin, vec3 translation, physicsQueryFilter filter)
{
	b3Pos b_origin = b3ToPos((b3Vec3){origin.x, origin.y, origin.z});
	b3Vec3 b_translation = (b3Vec3){translation.x, translation.y, translation.z};
	b3QueryFilter b_filter = b3DefaultQueryFilter();
	b_filter.categoryBits = filter.category;
	b_filter.maskBits = filter.mask;

	auto res = b3World_CastRayClosest(world, b_origin, b_translation, b_filter);
	return
	{
		res.shapeId,
		vec3{res.point.x, res.point.y, res.point.z},
		vec3{res.normal.x, res.normal.y, res.normal.z},
		res.fraction,
		res.hit
	};
}

void physics_world_cast_shape(vec3 origin, const physicsShapeProxy& proxy, vec3 translation, physicsQueryFilter filter, physicsCastCallback callback, void* callback_context)
{
	ZoneScoped;

	b3Pos b_origin = b3ToPos((b3Vec3){origin.x, origin.y, origin.z});
	b3Vec3 b_translation = (b3Vec3){translation.x, translation.y, translation.z};
	b3QueryFilter b_filter = b3DefaultQueryFilter();
	b_filter.categoryBits = filter.category;
	b_filter.maskBits = filter.mask;

	b3World_CastShape(world, b_origin, &proxy, b_translation, b_filter, callback, callback_context);
}

float physics_world_cast_mover(vec3 origin, const physicsCapsule& capsule, vec3 translation, physicsQueryFilter filter)
{
	ZoneScoped;

	b3Pos b_origin = b3ToPos((b3Vec3){origin.x, origin.y, origin.z});
	b3Vec3 b_translation = (b3Vec3){translation.x, translation.y, translation.z};
	b3QueryFilter b_filter = b3DefaultQueryFilter();
	b_filter.categoryBits = filter.category;
	b_filter.maskBits = filter.mask;

	const b3Capsule* b_capsule = reinterpret_cast<const b3Capsule*>(&capsule);
	return b3World_CastMover(world, b_origin, b_capsule, b_translation, b_filter, nullptr, nullptr); 
}

void physics_world_collide_mover(vec3 origin, const physicsCapsule& capsule, physicsQueryFilter filter, physicsPlaneCallback callback, void* context)
{
	ZoneScoped;

	b3Pos b_origin = b3ToPos((b3Vec3){origin.x, origin.y, origin.z});
	b3QueryFilter b_filter = b3DefaultQueryFilter();
	b_filter.categoryBits = filter.category;
	b_filter.maskBits = filter.mask;

	const b3Capsule* b_capsule = reinterpret_cast<const b3Capsule*>(&capsule);
	b3World_CollideMover(world, b_origin, b_capsule, b_filter, callback, context);
}

physicsPlaneSolverResult physics_solve_planes(vec3 target_delta, physicsCollisionPlane* planes, int count)
{
	ZoneScoped;
	return b3SolvePlanes((b3Vec3){target_delta.x, target_delta.y, target_delta.z}, planes, count);
}

vec3 physics_clip_vector(vec3 vector, const physicsCollisionPlane* planes, int count)
{
	ZoneScoped;
	b3Vec3 result = b3ClipVector((b3Vec3){vector.x, vector.y, vector.z}, planes, count);
	return vec3{result.x, result.y, result.z};
}

void physics_body_set_target_transform(physicsBody body, vec3 position, Quaternion rotation, float timestep, bool wake)
{	
	b3WorldTransform target
	{
		.p = b3ToPos(b3Vec3{position.x, position.y, position.z}),
		.q = b3Quat{b3Vec3{rotation.x, rotation.y, rotation.z}, rotation.w}
	};
	
	b3Body_SetTargetTransform(body, target, timestep, wake);
}

u64 physics_shape_get_userdata(physicsShape shape)
{
	return (u64)b3Shape_GetUserData(shape);
}

}
