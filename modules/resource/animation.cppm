export module penumbra.resource:animation;

import std;

using std::uint32_t;

export namespace penumbra
{

struct AnimationFileFormat
{
	constexpr static uint32_t fmt_magic = 0x4d4e414c; //FIXME
	constexpr static uint32_t fmt_major_version = 0u;
	constexpr static uint32_t fmt_minor_version = 1u;

	struct Header
	{
		uint32_t magic{fmt_magic};
		uint32_t vmajor{fmt_major_version};
		uint32_t vminor{fmt_minor_version};
		uint32_t channel_count;
		uint32_t ref_skeleton_offset;
		uint32_t channel_table_offset;
	};

	struct Channel
	{
		uint32_t keyframe_count;
		uint32_t timestamp_offset;
		uint32_t value_offset;
		uint32_t bone;
		uint32_t path;
		uint32_t interp;
	};
};

}
