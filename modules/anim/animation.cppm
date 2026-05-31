module;

#include <cassert>

export module penumbra.anim:animation;

import penumbra.math;
import std;

export namespace penumbra
{

enum class AnimationPath
{
	Translation,
	Rotation,
	Scale
};

enum class AnimationInterp
{
	Constant,
	Linear,
	CubicSpline
};

struct AnimationChannel
{
	std::uint32_t bone;
	AnimationPath path;
	AnimationInterp interp;

	std::vector<float> timestamps;
	std::vector<float> values;

	std::span<vec3> values_as_vec3()
	{
		assert(path == AnimationPath::Translation || path == AnimationPath::Scale);
		return {reinterpret_cast<vec3*>(values.data()), values.size() / 3};
	}

	std::span<Quaternion> values_as_quat()
	{
		assert(path == AnimationPath::Rotation);
		return {reinterpret_cast<Quaternion*>(values.data()), values.size() / 4};
	}
};

struct Animation
{
	std::string name;

	std::vector<AnimationChannel> channels;

	float start_time = std::numeric_limits<float>::max();
	float end_time = std::numeric_limits<float>::lowest();
};

}
