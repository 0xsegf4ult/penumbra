module;

#include <cassert>
#include <tracy/Tracy.hpp>

module penumbra.physics:bvh4_tree_build;

import :bvh4_node;
import :rigidbody;
import penumbra.math;
import std;

using std::uint32_t;

namespace penumbra
{

export struct tree_build_result 
{
	bvh4_node_id root;
	AABB bounds;
};

export struct tree_build_context
{
	pool_allocator<bvh4_node>& alloc;
	std::span<bvh4_node_id> nodes;
	uint32_t force_dirty_level{0};
};

struct tree_node_data
{
	uint32_t handle;
	AABB bounds;
};

struct tree_build_range
{
	bvh4_node_id node;
	AABB bounds;
	uint32_t first;
	uint32_t count;
	uint32_t level;
};

export tree_build_result bvh4_build_tree(tree_build_context&& ctx)
{
	ZoneScoped;
	assert(!ctx.nodes.empty());

	auto get_node_bounds = [&ctx](bvh4_node_id id) -> AABB
	{
		ZoneScoped;
		if(id.is_body())
			return physics_read_rigidbody(id.as_body())->get_transformed_bounds();

		const bvh4_node& n = ctx.alloc.get(id.as_node());
		AABB bnd = n.get_child_bounds(0);
		for(uint32_t c = 1; c < 4; c++)
			bnd = AABB::merge(bnd, n.get_child_bounds(c));

		return bnd;
	};

	if(ctx.nodes.size() == 1)
	{
		if(ctx.nodes[0].is_node())
			ctx.alloc.get(ctx.nodes[0].as_node()).parent = bvh4_node::invalid;

		return {ctx.nodes[0], get_node_bounds(ctx.nodes[0])};
	}

	const auto root = ctx.alloc.allocate();
	bvh4_node& root_node = ctx.alloc.get(root);
	root_node.invalidate();
	root_node.dirty = (ctx.force_dirty_level > 0u);

	auto node_data = std::make_unique_for_overwrite<tree_node_data[]>(ctx.nodes.size());

	AABB root_bounds{vec3{1e30f}, vec3{-1e30f}};

	{
	ZoneScopedN("collect_nodes");
	for(uint32_t i = 0; i < ctx.nodes.size(); i++)
	{
		const AABB rbounds = get_node_bounds(ctx.nodes[i]);
	       	node_data[i].handle = ctx.nodes[i];
		node_data[i].bounds = rbounds;
		root_bounds = AABB::merge(root_bounds, rbounds);
	}

	}	

	auto b_stack = std::make_unique_for_overwrite<tree_build_range[]>(128);
	b_stack[0] = {bvh4_node_id::from_node(root), root_bounds, 0, uint32_t(ctx.nodes.size()), 0};
	uint32_t b_stack_top = 0;

	auto spatial_partition_quad = [&node_data](tree_build_range& r)
	{
		ZoneScoped;
		auto spatial_partition_split = [&node_data](tree_build_range& range, bool increment_level = true)
		{
			ZoneScoped;
			AABB hlb{vec3{1e30f}, vec3{-1e30f}};
			AABB hrb = hlb;

			if(!range.count)
				return std::make_pair(tree_build_range{{}, hlb, range.first, 0, 0}, tree_build_range{{}, hrb, range.first, 0, 0});

			{
			ZoneScopedN("range_bounds");
			for(uint32_t i = range.first; i < range.first + range.count; i++)
				range.bounds = AABB::merge(range.bounds, node_data[i].bounds);
			}

			uint32_t axis = 0;
			const vec3 len = range.bounds.maxs - range.bounds.mins;
			if(len.y > len.x)
				axis = 1;
			if(len.z > len.y && len.z > len.x)
				axis = 2;

			const float plane = 0.5f * (range.bounds.mins + range.bounds.maxs)[axis];

			tree_node_data* part_begin;

			{
			ZoneScopedN("partition");
			part_begin = std::partition(node_data.get() + range.first, node_data.get() + range.first + range.count, [axis, plane](const tree_node_data& in)
			{
				return (0.5f * (in.bounds.mins + in.bounds.maxs))[axis] < plane;
			});
			}

			uint32_t hl_count = uint32_t(part_begin - (node_data.get() + range.first));

			{
			ZoneScopedN("build_range_s0");
			for(uint32_t i = range.first; i < range.first + hl_count; i++)
				hlb = AABB::merge(hlb, node_data[i].bounds);
			}

			const tree_build_range hl{{}, hlb, range.first, hl_count, range.level + increment_level};
			{
			ZoneScopedN("build_range_s1");
			for(uint32_t i = range.first + hl_count; i < range.first + range.count; i++)
				hrb = AABB::merge(hrb, node_data[i].bounds);
			}

			const tree_build_range hr{{}, hrb, range.first + hl_count, range.count - hl_count, range.level + increment_level};

			return std::make_pair(hl, hr);
		};

		auto [hl, hr] = spatial_partition_split(r, true);

		auto [q0, q1] = spatial_partition_split(hl, false);
		auto [q2, q3] = spatial_partition_split(hr, false);

		return std::array<tree_build_range, 4>{q0, q1, q2, q3};
	};

	{
	ZoneScopedN("build_tree_stackwalk");

	for(;;)
	{
		if(b_stack_top >= 128)
		{
			log::warn("bvh4_build_tree: out of stack space");
			break;
		}

		tree_build_range r = b_stack[b_stack_top];
		bvh4_node& node = ctx.alloc.get(r.node.as_node());

		if(r.count <= 4)
		{
			for(uint32_t i = r.first; i < r.first + r.count; i++)
			{
				const bvh4_node_id cid = node_data[i].handle;
				node.children[i - r.first] = cid;
				node.set_child_bounds(i - r.first, node_data[i].bounds);
				if(cid.is_body())
					physics_readwrite_rigidbody(cid.as_body())->userdata = (static_cast<uint64_t>(r.node) << 32) | (i - r.first);
				else
					ctx.alloc.get(cid.as_node()).parent = r.node;
			}
		}
		else
		{
			auto quads = spatial_partition_quad(r);

			for(uint32_t i = 0; i < 4; i++)
			{
				if(quads[i].count)
				{
					if(quads[i].count == 1)
					{
						const bvh4_node_id cid = node_data[quads[i].first].handle;
						node.children[i] = cid;
						node.set_child_bounds(i, quads[i].bounds);
						if(cid.is_body())
							physics_readwrite_rigidbody(cid.as_body())->userdata = (static_cast<uint64_t>(r.node) << 32) | i;
						else
							ctx.alloc.get(cid.as_node()).parent = r.node;
					}
					else
					{
						quads[i].node = bvh4_node_id::from_node(ctx.alloc.allocate());
						bvh4_node& qn = ctx.alloc.get(quads[i].node.as_node());
						qn.invalidate();
						qn.parent = r.node;
						qn.dirty = ctx.force_dirty_level > quads[i].level;
						node.children[i] = quads[i].node;
						node.set_child_bounds(i, quads[i].bounds);
						b_stack[b_stack_top++] = quads[i];
					}
				}
			}
		}

		if(b_stack_top == 0)
			break;

		b_stack_top--;
	}
	}

	return {bvh4_node_id::from_node(root), root_bounds};
}

}
