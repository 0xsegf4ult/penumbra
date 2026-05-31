export module penumbra.anim:skeleton;

import penumbra.math;
import std;

export namespace penumbra
{

struct Skeleton
{
	std::string name;

	std::vector<std::string> bone_names;
	std::vector<Transform> bone_transforms;
	std::vector<std::uint16_t> bone_parents;
	std::vector<mat4> bone_inv_bind_matrices;

	std::uint16_t bone_count;
};

}
