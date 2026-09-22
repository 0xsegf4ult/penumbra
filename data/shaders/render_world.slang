#pragma once

[Flags]
enum ObjectFlags : uint
{
	Disabled
};

struct GPUWorldRenderable
{
	float4x4 transform;
	float4 sphere;
	float cull_scale;
	uint material_offset;
	uint lod_offset;
	uint pack_bucket_lcount;
	uint geom_vtx_offset;
	uint geom_idx_offset;
	uint geom_cluster_offset;
	uint flags;
};

[Flags]
enum LightFlags : uint
{
	PointLight,
	SpotLight,
	Disabled,
	Shadowcast
};

struct GPUWorldLight
{
	float4 pos_ic;
	float4 color_rad;
	float4 dir_oc;
	uint4 flags_unused;
};

struct GeometryLOD
{
	uint cluster_offset;
	uint cluster_count;
};

struct GeometryCluster
{
	int vertex_offset;
	uint vertex_count;
	uint index_offset;
	uint index_count;

	float4 sphere;
	float4 cone;
};

struct DrawIndirectCommand
{
	uint vertex_count;
	uint instance_count;
	uint first_vertex;
	uint first_instance;	
};

struct DrawIndexedIndirectCommand
{
	uint index_count;
	uint instance_count;
	uint first_index;
	uint first_vertex;
	uint first_instance;
};

struct VisCounters
{
	uint visible_meshlets;
	uint threadgroup_count;
};

struct GPUWorldBucket
{
	uint offset;
	uint counter;
};

struct GPUWorldMeshlet
{
	uint renderable_index;
	uint bucket;
	uint lod;
	uint meshlet_index;
};

struct GPUWorldMeshletInstance
{
	uint renderable_index;
	uint meshlet_index;
};

[Flags]
enum CullingFlags : uint
{
	FrustumCull,
	ConeCull,
	ContributionCull,
	OcclusionCull,
	IsOrtho
};

struct CullingData
{
	float4x4 viewmat;	
	float4 cam_pos;
	float4 frustum_planes[4];
	float lod_base;
	float lod_step;
	float znear;
	float zfar;
	float projX;
	float projY;
	float min_contrib;	
	CullingFlags flags;
	uint lod_bias;
};

