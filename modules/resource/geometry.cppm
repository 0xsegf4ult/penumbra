export module penumbra.resource:geometry;

import penumbra.math;
import std;

using std::uint32_t, std::int32_t, std::uint64_t;

namespace penumbra
{

export struct GeometryFileFormat
{
	constexpr static uint32_t fmt_magic = 0x4c444d4c; //FIXME 
        constexpr static uint32_t fmt_major_version = 2u;
        constexpr static uint32_t fmt_minor_version = 1u;

	enum class VertexFormat : uint32_t
	{
		Static,
		Skinned
	};

	struct Header
        {
                uint32_t magic{fmt_magic};
                uint32_t vmajor{fmt_major_version};
                uint32_t vminor{fmt_minor_version};
                VertexFormat vert_format;
                uint32_t vpos_offset;
                uint32_t vuv_offset;
                uint32_t vnorms_offset;
                uint32_t index_offset;
                uint32_t lod_offset;
                uint32_t num_lods;
                uint32_t cluster_offset;
                vec4 sphere;
        };

	struct LOD
        {
                uint32_t cluster_offset{0u};
                uint32_t cluster_count{0u};
        };

        struct Cluster
        {
                int32_t vertex_offset{0};
                uint32_t vertex_count{0u};
                uint32_t index_offset{0u};
                uint32_t index_count{0u};

                vec4 sphere;
                vec4 cone;
        };
};

export struct GeometryResource
{
	uint32_t vertex_offset;
	uint32_t vertex_count;
	uint32_t index_offset;
	uint32_t index_count;
	uint32_t cluster_offset;
	uint32_t cluster_count;
	uint32_t l0_cluster_count;
	uint32_t lod_offset;
	uint32_t lod_count;
	vec4 sphere;
	uint64_t syncval;
};

}
