module;

#include <cassert>
#include <tracy/Tracy.hpp>

module penumbra.physics:bvh4_tree;

import :bvh4_node;
import :bvh4_tree_build;
import :rigidbody;
import penumbra.core;
import penumbra.math;
import std;

using std::uint32_t;

namespace penumbra 
{

export class bvh4_tree
{
public:
	using allocator_t = pool_allocator<bvh4_node>;

	constexpr bvh4_tree() = default;

	void init(allocator_t* alloc)
	{
		allocator = alloc;
		root_nodes[0] = allocator->allocate();
		allocator->get(root_nodes[0]).invalidate();
	}

	void insert_bodies(std::span<rigidbody_handle> bodies)
	{
		ZoneScoped;

		auto [subtree_root, subtree_bounds] = bvh4_build_tree
		({
			.alloc = *allocator,
			.nodes = {reinterpret_cast<bvh4_node_id*>(bodies.data()), bodies.size()},
			.force_dirty_level = 0u
		});
	
		dirty = true;	

		tree_bodies += bodies.size();

		for(;;)
		{
			ZoneScopedN("try_insert");

			if(insert_subtree(subtree_root, subtree_bounds))
				return;

			if(insert_new_root(subtree_root, subtree_bounds))
				return;
		}
	}

	void update_bodies(std::span<rigidbody_handle> bodies)
	{
		ZoneScoped;

		for(auto body : bodies)
		{
			AABB rbounds;
			uint64_t data;

			// using reader lock
			{
				auto rb = physics_read_rigidbody(body);
				rbounds = rb->get_transformed_bounds();
				data = rb->userdata;
			}

			const bvh4_node_id nid = data >> 32;
			const uint32_t cid = data & 0x3;

			assert(nid.is_node());

			bvh4_node& node = allocator->get(nid.as_node());
			if(node.enlarge_child_bounds(cid, rbounds))
			{
				dirty = true;
				propagate_node_bounds(nid.as_node(), rbounds);
			}
		}
	}

	void remove_bodies(std::span<rigidbody_handle> bodies)
	{
		ZoneScoped;

		dirty = true;

		for(auto body : bodies)
		{
			uint64_t data;
			
			// using read-write lock
			{
				auto rb = physics_readwrite_rigidbody(body);
				data = rb->userdata;
				rb->userdata = uint64_t(bvh4_node::invalid) << 32;
			}

			const bvh4_node_id nid = data >> 32;
			const uint32_t cid = data & 0x3;

			assert(nid.is_node());

			bvh4_node& node = allocator->get(nid.as_node());
			node.invalidate_child_bounds(cid);
			node.children[cid] = bvh4_node::invalid;
			propagate_dirty_flag(nid.as_node());
		}

		tree_bodies -= bodies.size();
	}

	void rebuild_tree()
	{
		ZoneScoped;

		dirty = false;

		const uint32_t root = get_current_root();

		std::array<bvh4_node_id, 128> n_stack;
		uint32_t n_stack_top = 0;
		n_stack[0] = bvh4_node_id::from_node(root);

		//FIXME: allocate from some type of bump allocator
		auto ntree_nodes = std::make_unique_for_overwrite<bvh4_node_id[]>(tree_bodies);
		uint32_t ntree_top = 0;

		{
		ZoneScopedN("collect_dirty");
		for(;;)
		{
			const bvh4_node_id id = n_stack[n_stack_top];
			if(id.is_body())
			{
				ntree_nodes[ntree_top++] = id;
			}
			else
			{
				const auto& node = allocator->get(id.as_node());

				if(node.dirty)
				{
					for(const auto& child : node.children)
					{
						if(child == bvh4_node::invalid)
							continue;

						if(n_stack_top == 128)
						{
							log::warn("rebuild_tree: out of stack space");
							break;
						}

						n_stack[n_stack_top++] = bvh4_node_id{child};
					}

					discard_nodes.push_back(id.as_node());
				}
				else
				{
					ntree_nodes[ntree_top++] = id;
				}
			}

			if(n_stack_top == 0)
				break;

			n_stack_top--;
		}
		}

		bvh4_node_id new_root;
		if(ntree_top == 0)
		{
			new_root = bvh4_node_id::from_node(allocator->allocate());
			allocator->get(new_root.as_node()).invalidate();
		}
		else
		{
			auto [rnode, rbounds] = bvh4_build_tree
			({
				.alloc = *allocator,
				.nodes = {ntree_nodes.get(), ntree_top},
				.force_dirty_level = 5u
			});

			if(rnode.is_body())
			{
				new_root = allocator->allocate();
				bvh4_node& nr = allocator->get(new_root.as_node());
				nr.invalidate();
				nr.set_child_bounds(0, rbounds);
				nr.children[0] = rnode;

				physics_readwrite_rigidbody(rnode.as_body())->userdata = (static_cast<uint64_t>(new_root) << 32);
			}
			else
			{
				new_root = rnode;
			}
		}

		root_switch_target = new_root.as_node();
	}

	void switch_root()
	{
		ZoneScoped;
		if(root_switch_target == bvh4_node::invalid)
			return;

		const uint32_t new_root = (active_root + 1) % 2;
		root_nodes[new_root].store(root_switch_target.load());
		root_switch_target = bvh4_node::invalid;
		active_root = new_root;

		{
		ZoneScopedN("discard_nodes");
		for(auto node : discard_nodes)
			allocator->free(node);

		discard_nodes.clear();
		}
	}

	constexpr bool is_dirty() const
	{
		return dirty;
	}
private:	
	bool insert_subtree(bvh4_node_id node, AABB& bounds)
	{
		ZoneScoped;

		auto root = get_current_root();
		bvh4_node& rnode = allocator->get(root);

		if(node.is_node())
			allocator->get(node.as_node()).parent = root;

		for(uint32_t i = 0; i < 4; i++)
		{
			uint32_t inv = bvh4_node::invalid;
			if(rnode.children[i].compare_exchange_strong(inv, uint32_t(node)))
			{
				rnode.set_child_bounds(i, bounds);
				rnode.dirty = true;
				if(node.is_body())
				{
					physics_readwrite_rigidbody(node.as_body())->userdata = (static_cast<uint64_t>(root) << 32) | i;
				}
				
				return true;
			}
		}

		return false;
	}

	bool insert_new_root(bvh4_node_id node, AABB& bounds)
	{
		ZoneScoped;

		auto new_root = allocator->allocate();
		bvh4_node& nr_node = allocator->get(new_root);
		nr_node.invalidate();
		nr_node.dirty = true;

		auto root = get_current_root();
		bvh4_node& rnode = allocator->get(root);

		if(node.is_node())
		{
			allocator->get(node.as_node()).parent = new_root;
		}
		else
		{
			physics_readwrite_rigidbody(node.as_body())->userdata = (static_cast<uint64_t>(new_root) << 32) | 1;
		}

		nr_node.children[0] = root;
		nr_node.set_child_bounds(0, AABB{vec3{-1e30f}, vec3{1e30f}});
		nr_node.children[1] = node;
		nr_node.set_child_bounds(1, bounds);

		if(root_nodes[active_root].compare_exchange_strong(root, new_root))
		{
			rnode.parent = new_root;
			return true;
		}

		allocator->free(new_root);
		return false;
	}

	void propagate_dirty_flag(uint32_t node)
	{
		ZoneScoped;
		uint32_t idx = node;

		for(;;)
		{
			if(idx == bvh4_node::invalid)
				return;

			bvh4_node& n = allocator->get(idx);
			if(n.dirty)
				break;

			n.dirty = true;
			idx = n.parent;
		}
	}

	void propagate_node_bounds(uint32_t node, const AABB& bounds)
	{
		ZoneScoped;
		uint32_t idx = node;

		for(;;)
		{
			bvh4_node& n = allocator->get(idx);
			n.dirty = true;

			uint32_t pid{n.parent};
			assert(pid != idx && "Node is parented to itself!");

			if(pid == bvh4_node::invalid)
				return;

			bvh4_node& parent = allocator->get(pid);
			uint32_t child = 4;
			for(uint32_t i = 0; i < 4; i++)
			{
				if(parent.children[i] == idx)
				{
					child = i;

					if(!parent.enlarge_child_bounds(child, bounds))
					{
						if(!parent.dirty)
							propagate_dirty_flag(pid);
						return;
					}

					break;
				}
			}

			idx = pid;
		}
	}

	allocator_t* allocator{nullptr};

	uint32_t get_current_root()
	{
		return root_nodes[active_root];
	}

	std::array<std::atomic<uint32_t>, 2> root_nodes;
	std::atomic<uint32_t> active_root{0u};
	std::atomic<uint32_t> root_switch_target{bvh4_node::invalid};

	bool dirty{false};
	uint32_t tree_bodies{0u};
	std::vector<uint32_t> discard_nodes;
};

}
