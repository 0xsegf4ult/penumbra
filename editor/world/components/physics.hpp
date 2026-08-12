#pragma once

#include <penumbra/physics.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

struct rigidbody_component
{
	physicsBodyDesc desc;
	physicsBodyID handle;
};

struct sphere_collider_component
{
	physicsSphere desc;
};

struct capsule_collider_component
{
	physicsCapsule desc;
};

struct box_collider_component
{
	vec3 half_size;
};

}
