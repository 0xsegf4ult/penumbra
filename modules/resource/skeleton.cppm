export module penumbra.resource:skeleton;

import std;

using std::uint32_t;

export namespace penumbra
{

struct SkeletonFileFormat
{
	constexpr static uint32_t fmt_magic = 0x4c4b534c; //FIXME
	constexpr static uint32_t fmt_major_version = 0u;
	constexpr static uint32_t fmt_minor_version = 1u;

	struct Header
	{
		uint32_t magic{fmt_magic};
		uint32_t vmajor{fmt_major_version};
		uint32_t vminor{fmt_minor_version};
		uint32_t bone_count;
		uint32_t name_table_offset;
		uint32_t transform_table_offset;
		uint32_t parent_table_offset;
		uint32_t matrix_table_offset;
	};
};

}
