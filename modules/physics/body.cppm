module;

#include <cassert>

module penumbra.physics:rigidbody;

import :shape;
import penumbra.core;
import penumbra.math;
import std;

using std::uint32_t, std::uint64_t;

namespace penumbra
{

struct Rigidbody
{
	constexpr static uint32_t broadphase_node_bit = (1u << 30);
	constexpr static uint32_t handle_mask = 0x3FFFFFFF;

	Transform transform;

	vec3 velocity{0.0f};
	vec3 angular_velocity{0.0f};

	vec3 forces{0.0f};
	vec3 torque{0.0f};

	float inverse_mass = 1.0f;

	mat3 inv_inertia_tensor_world;
	mat3 inv_inertia_tensor_local;

	physicsShapeRef collider;
	physicsBodyType body_type;
	physicsMotionType motion_type;
	uint64_t broadphase_data;
	uint64_t userdata;
       			
        [[nodiscard]] constexpr AABB get_transformed_bounds() const
        {
                vec3 mins = collider->bounds.mins + transform.translation;
                vec3 maxs = collider->bounds.maxs + transform.translation;

                if(transform.rotation != Quaternion{0.0f, 0.0f, 0.0f, 1.0f})
                {
                        const vec3 xn{-1.0f, 0.0f, 0.0f};
                        const vec3 yn{0.0f, -1.0f, 0.0f};
                        const vec3 zn{0.0f, 0.0f, -1.0f};
                        const vec3 xp{1.0f, 0.0f, 0.0f};
			const vec3 yp{0.0f, 1.0f, 0.0f};
                        const vec3 zp{0.0f, 0.0f, 1.0f};

                        mat4 local_to_world = transform.as_matrix();
                        const mat3 world_to_local = mat3::transpose(Quaternion::make_mat3(transform.rotation));

                        const vec3 xn_loc = (xn * world_to_local);
                        const vec3 yn_loc = (yn * world_to_local);
                        const vec3 zn_loc = (zn * world_to_local);
                        const vec3 xp_loc = (xp * world_to_local);
                        const vec3 yp_loc = (yp * world_to_local);
                        const vec3 zp_loc = (zp * world_to_local);

                        mins.x = (vec4{collider->get_point(collider->get_support(xn_loc)), 1.0f} * local_to_world).x;
                        mins.y = (vec4{collider->get_point(collider->get_support(yn_loc)), 1.0f} * local_to_world).y;
                        mins.z = (vec4{collider->get_point(collider->get_support(zn_loc)), 1.0f} * local_to_world).z;
                        maxs.x = (vec4{collider->get_point(collider->get_support(xp_loc)), 1.0f} * local_to_world).x;
                        maxs.y = (vec4{collider->get_point(collider->get_support(yp_loc)), 1.0f} * local_to_world).y;
                        maxs.z = (vec4{collider->get_point(collider->get_support(zp_loc)), 1.0f} * local_to_world).z;
                }

                return AABB{mins, maxs};
        }
};


}
