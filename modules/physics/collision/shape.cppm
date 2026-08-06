export module penumbra.physics:shape;

import penumbra.core;
import penumbra.math;
import std;

using std::size_t, std::uint8_t;

namespace penumbra
{

export enum physicsShapeType
{
	PHYSICS_SHAPE_SPHERE,
	PHYSICS_SHAPE_CAPSULE,
	PHYSICS_SHAPE_HULL
};

export struct physicsShapeDesc
{
	float density = 1.0f;
};

export class physicsShape : public RefCountEnabled<physicsShape>
{
public:
	physicsShape() = default;
	physicsShape(physicsShapeType t) : type{t} {}

	virtual ~physicsShape() {}

	virtual size_t get_support(const vec3& dir) const = 0;
	virtual vec3 get_point(size_t index) const = 0;
	virtual float get_convex_radius() const = 0;

	physicsShapeType type;
	float density{1.0f};
	float mass{1.0f};
	AABB bounds;
	mat3 inertia_tensor;
};

export using physicsShapeRef = RefCounted<physicsShape>;

export struct physicsSphereDesc : public physicsShapeDesc
{
	float radius;
};

export class physicsSphereShape final : public physicsShape
{
public:
	physicsSphereShape() : physicsShape(PHYSICS_SHAPE_SPHERE) {}

	size_t get_support(const vec3&) const override
	{
		return 0;
	}

	vec3 get_point(size_t index) const override
	{
		return vec3{0.0f};
	}

	float get_convex_radius() const override
	{
		return radius;
	}

	float radius;
};

export struct physicsCapsuleDesc : public physicsShapeDesc
{
	float radius;
	float height;
};

export class physicsCapsuleShape final : public physicsShape
{
public:
	physicsCapsuleShape() : physicsShape(PHYSICS_SHAPE_CAPSULE) {}

	size_t get_support(const vec3& dir) const override
	{
		return (dir.y > 0.0f) ? 0 : 1;
	}

	vec3 get_point(size_t index) const override
	{
		const float hh = height * 0.5f;
		return vec3{0.0f, (index == 0) ? hh : -hh, 0.0f};
	}

	float get_convex_radius() const override
	{
		return radius;
	}

	float radius;
	float height;
};

export struct physicsBoxDesc : public physicsShapeDesc
{
	vec3 edges;
};

export struct hull_halfedge
{
	uint8_t next;
	uint8_t twin;
	uint8_t origin;
	uint8_t face;
};

export struct physicsHullShape final : public physicsShape
{
public:
	physicsHullShape() : physicsShape(PHYSICS_SHAPE_HULL) {}

	size_t get_support(const vec3& dir) const override
	{
		size_t index{0u};
		float maxproj = vec3::dot(dir, vertices[index]);
		for(size_t i = 1u; i < vertices.size(); i++)
		{
			const float proj = vec3::dot(dir, vertices[i]);
			if(proj > maxproj)
			{
				index = i;
				maxproj = proj;
			}
		}

		return index;
	}

	vec3 get_point(size_t index) const override
	{
		return vertices[index];
	}

	float get_convex_radius() const override
	{
		return 0.0f;
	}

	std::vector<vec3> vertices;
	std::vector<Plane> planes;
	std::vector<hull_halfedge> edges;
	std::vector<std::uint32_t> plane_to_edge;
};

}
