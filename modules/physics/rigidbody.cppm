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

enum class BodyType
{
	Static,
	Kinematic,
	Dynamic
};

enum class MotionType
{
	Discrete,
	LinearCCD
};

struct RigidbodyDescription
{
	Transform initial_transform;
	const RefCounted<physicsShape>& shape;
	BodyType body_type{BodyType::Dynamic};
	MotionType motion_type{MotionType::Discrete};
};

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

	RefCounted<physicsShape> collider;
	BodyType body_type;
	MotionType motion_type;
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

struct rigidbody_storage
{
	constexpr static std::size_t mutex_slots = 32;

	std::vector<Rigidbody> bodies;
	std::vector<uint32_t> freelist;

	std::array<std::shared_mutex, mutex_slots> body_locks;
};

rigidbody_storage* storage = nullptr;

using rigidbody_handle = strongly_typed<uint32_t, struct _rb_handle_tag>;

void physics_init_rigidbody_storage()
{
	storage = new rigidbody_storage();
}

void physics_shutdown_rigidbody_storage()
{
	delete storage;
}

rigidbody_handle physics_create_rigidbody(const RigidbodyDescription& desc)
{
	Rigidbody rb;
	rb.collider = desc.shape;
	rb.transform = desc.initial_transform;
	rb.body_type = desc.body_type;
	rb.motion_type = desc.motion_type;

	const float mass = rb.collider->mass;

	if(mass == 0.0f)
		rb.inverse_mass = 0.0f;
	else
		rb.inverse_mass = 1.0f / mass;

	mat3 inertia_tensor = rb.collider->inertia_tensor;

	mat3 rotM = Quaternion::make_mat3(rb.transform.rotation);
	inertia_tensor = mat3::transpose(rotM) * inertia_tensor * rotM;

	rb.inv_inertia_tensor_local = mat3::inverse(inertia_tensor);
	
	storage->bodies.push_back(rb);
	auto alloc = uint32_t(storage->bodies.size());
	return rigidbody_handle{alloc | Rigidbody::broadphase_node_bit};	
}

template <bool is_rw>
class rigidbody_access
{
public:
	constexpr rigidbody_access(rigidbody_handle h) : handle{h} {};
	~rigidbody_access()
	{
		if(!handle)
			return;

		const auto index = (handle & Rigidbody::handle_mask) - 1;
		const auto lock_index = index % rigidbody_storage::mutex_slots;
		if constexpr(is_rw)
			storage->body_locks[lock_index].unlock();
		else
			storage->body_locks[lock_index].unlock_shared();
	}

	rigidbody_access(const rigidbody_access&) = delete;
	rigidbody_access(rigidbody_access&&) = delete;

	rigidbody_access& operator=(const rigidbody_access&) = delete;
	rigidbody_access& operator=(rigidbody_access&&) = delete;

	const Rigidbody& operator*() const
	{
		const auto index = (handle & Rigidbody::handle_mask) - 1;
		return storage->bodies[index];
	}
	
	Rigidbody& operator*() requires is_rw
	{
		const auto index = (handle & Rigidbody::handle_mask) - 1;
		return storage->bodies[index];
	}

	const Rigidbody* operator->() const
	{
		const auto index = (handle & Rigidbody::handle_mask) - 1;
		return &storage->bodies[index];
	}

	Rigidbody* operator->() requires is_rw
	{
		const auto index = (handle & Rigidbody::handle_mask) - 1;
		return &storage->bodies[index];
	}
private:
	rigidbody_handle handle;
};

rigidbody_access<false> physics_read_rigidbody(rigidbody_handle handle)
{
	assert(handle & Rigidbody::handle_mask);
	const auto index = (handle & Rigidbody::handle_mask) - 1;
	const auto lock_index = index % rigidbody_storage::mutex_slots;
	storage->body_locks[lock_index].lock_shared();
	return {handle};
}

rigidbody_access<true> physics_readwrite_rigidbody(rigidbody_handle handle)
{
	assert(handle & Rigidbody::handle_mask);
	const auto index = (handle & Rigidbody::handle_mask) - 1;
	const auto lock_index = index % rigidbody_storage::mutex_slots;
	storage->body_locks[lock_index].lock();
	return {handle};
}

}

