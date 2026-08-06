module penumbra.physics:broadphase;

import :bvh4_tree;
import :rigidbody;
import penumbra.core;
import std;

namespace penumbra
{

export class phys_broadphase
{
public:
	phys_broadphase() : allocator{"bvh4_node_allocator"}
	{
		const uint32_t est_max_nodes = 512;
		allocator.init(2 * est_max_nodes, 2 * est_max_nodes);

		num_layers = 1u;
		layers = new bvh4_tree[num_layers];
		for(uint32_t i = 0; i < num_layers; i++)
			layers[i].init(&allocator);
	}

	~phys_broadphase()
	{
		delete[] layers;
	}

	phys_broadphase(const phys_broadphase&) = delete;
	phys_broadphase& operator=(const phys_broadphase&) = delete;

	phys_broadphase(phys_broadphase&&) = delete;
	phys_broadphase& operator=(phys_broadphase&&) = delete;

	void insert_bodies(std::span<rigidbody_handle> bodies)
	{
		layers[0].insert_bodies(bodies);
	}

	void update_bodies(std::span<rigidbody_handle> bodies)
	{
		layers[0].update_bodies(bodies);
	}

	void remove_bodies(std::span<rigidbody_handle> bodies)
	{
		layers[0].remove_bodies(bodies);
	}

	void prepare_update()
	{
		for(uint32_t i = 0; i < num_layers; i++)
		{
			if(layers[i].is_dirty())
				layers[i].rebuild_tree();
		}
	}

	void commit_update()
	{
		for(uint32_t i = 0; i < num_layers; i++)
			layers[i].switch_root();
	}
private:
	bvh4_tree::allocator_t allocator;
	bvh4_tree* layers{nullptr};
	uint32_t num_layers{0u};
};

}
