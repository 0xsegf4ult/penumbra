module;

#include <cassert>
#include <nmmintrin.h>

module penumbra.physics:bvh4_node;

import penumbra.math;
import std;

using std::uint32_t;

namespace penumbra
{

export struct bvh4_node
{
	constexpr static float invalid_bounds = 1e30f;
	constexpr static uint32_t is_rigidbody_node_bit = (1u << 30);
	constexpr static uint32_t node_handle_mask = 0x3FFFFFFF;
	constexpr static uint32_t invalid = 0xFFFFFFFF;

	std::atomic<float> bounds_minX[4];
	std::atomic<float> bounds_minY[4];
	std::atomic<float> bounds_minZ[4];

	std::atomic<float> bounds_maxX[4];
	std::atomic<float> bounds_maxY[4];
	std::atomic<float> bounds_maxZ[4];

	std::atomic<uint32_t> children[4];
	std::atomic<uint32_t> parent;
	std::atomic<uint32_t> dirty;
	uint32_t padding;

	void invalidate()
	{
		__m128 bounds = _mm_set1_ps(invalid_bounds);

		_mm_storeu_ps(reinterpret_cast<float*>(&bounds_minX[0]), bounds);
		_mm_storeu_ps(reinterpret_cast<float*>(&bounds_minY[0]), bounds);
		_mm_storeu_ps(reinterpret_cast<float*>(&bounds_minZ[0]), bounds);

		bounds = _mm_set1_ps(-invalid_bounds);

		_mm_storeu_ps(reinterpret_cast<float*>(&bounds_maxX[0]), bounds);
		_mm_storeu_ps(reinterpret_cast<float*>(&bounds_maxY[0]), bounds);
		_mm_storeu_ps(reinterpret_cast<float*>(&bounds_maxZ[0]), bounds);

		__m128i index = _mm_set1_epi32(static_cast<int>(invalid));
		_mm_storeu_si128(reinterpret_cast<__m128i*>(&children[0]), index);

		parent = invalid;
		dirty = false;
	}

	void set_child_bounds(uint32_t child, const AABB& bounds)
	{
		assert(child < 4u);

		bounds_maxZ[child] = bounds.maxs.z;
		bounds_maxY[child] = bounds.maxs.y;
		bounds_maxX[child] = bounds.maxs.x;
		bounds_minZ[child] = bounds.mins.z;
		bounds_minY[child] = bounds.mins.y;
		bounds_minX[child] = bounds.mins.x;
	}

	AABB get_child_bounds(uint32_t child) const noexcept
	{
		assert(child < 4u);

		return
		{
			vec3{bounds_minX[child], bounds_minY[child], bounds_minZ[child]},
			vec3{bounds_maxX[child], bounds_maxY[child], bounds_maxZ[child]}
		};
	}

	bool enlarge_child_bounds(uint32_t child, const AABB& bounds)
	{
		assert(child < 4u);

		bool dirty = false;

		dirty |= atomic_min(bounds_minX[child], bounds.mins.x);
	       	dirty |= atomic_min(bounds_minY[child], bounds.mins.y);
		dirty |= atomic_min(bounds_minZ[child], bounds.mins.z);
		dirty |= atomic_max(bounds_maxX[child], bounds.maxs.x);
		dirty |= atomic_max(bounds_maxY[child], bounds.maxs.y);
		dirty |= atomic_max(bounds_maxZ[child], bounds.maxs.z);

		return dirty;
	}

	void invalidate_child_bounds(uint32_t child)
	{
		assert(child < 4u);

		bounds_minX[child] = invalid_bounds;
		bounds_minY[child] = invalid_bounds;
		bounds_minZ[child] = invalid_bounds;
		bounds_maxX[child] = -invalid_bounds;
		bounds_maxY[child] = -invalid_bounds;
		bounds_maxZ[child] = -invalid_bounds;
	}
};	

export struct bvh4_node_id
{
	bvh4_node_id() : handle{bvh4_node::invalid} {}
	bvh4_node_id(uint32_t h) : handle{h} {}
	
	static bvh4_node_id from_node(uint32_t handle)
	{
		return bvh4_node_id{handle};
	}

	static bvh4_node_id from_body(uint32_t handle)
	{
		return bvh4_node_id(handle | bvh4_node::is_rigidbody_node_bit);
	}

	operator uint32_t() const { return handle; }

	[[nodiscard]] constexpr bool is_body() const noexcept
	{
		return handle & bvh4_node::is_rigidbody_node_bit;
	}

	[[nodiscard]] constexpr bool is_node() const noexcept
	{
		return !is_body();
	}

	[[nodiscard]] constexpr uint32_t as_node() const noexcept
	{
		assert(is_node());
		return handle;
	}

	[[nodiscard]] constexpr uint32_t as_body() const noexcept
	{
		assert(is_body());
		return handle & (~bvh4_node::is_rigidbody_node_bit);
	}
private:
	uint32_t handle;
};

}
