#pragma once

#include <penumbra/math/transform.hpp>
#include <penumbra/types.hpp>
#include <string>
#include <vector>

namespace penumbra
{

struct AnimationFileFormat
{
	constexpr static u32 fmt_magic = 0x4d4e414c; //FIXME
	constexpr static u32 fmt_major = 0u;
	constexpr static u32 fmt_minor = 1u;

	struct Header
	{
		u32 magic{fmt_magic};
		u32 vmajor{fmt_major};
		u32 vminor{fmt_minor};
		u32 channel_count;
		u32 ref_skeleton_offset;
		u32 channel_table_offset;
	};

	struct Channel
	{
		u32 keyframe_count;
		u32 timestamp_offset;
		u32 value_offset;
		u32 bone;
		u32 path;
		u32 interp;
	};
};

struct SkeletonFileFormat
{
	constexpr static u32 fmt_magic = 0x4c4b534c; //FIXME
	constexpr static u32 fmt_major = 0u;
	constexpr static u32 fmt_minor = 1u;

	struct Header
	{
		u32 magic{fmt_magic};
		u32 vmajor{fmt_major};
		u32 vminor{fmt_minor};
		u32 bone_count;
		u32 name_table_offset;
		u32 transform_table_offset;
		u32 parent_table_offset;
		u32 matrix_table_offset;
	};
};

enum animation_path_t
{
	ANIM_PATH_TRANSLATION,
	ANIM_PATH_ROTATION,
	ANIM_PATH_SCALE
};

enum animation_interp_t
{
	ANIM_INTERP_CONSTANT,
	ANIM_INTERP_LINEAR,
	ANIM_INTERP_CUBICSPLINE
};

struct anim_channel
{
	u32 bone;
	animation_path_t path;
	animation_interp_t interp;

	std::vector<float> timestamps;
	std::vector<float> values;
};

struct animation_resource
{
	std::string name;
	std::vector<anim_channel> channels;

	float start_time;
	float end_time;
};

struct skeleton_resource
{
	std::string name;

	std::vector<std::string> bone_names;
	std::vector<Transform> bone_transforms;
	std::vector<u16> bone_parents;
	std::vector<mat4> bone_inv_bind_matrices;

	u16 bone_count;
};

}
