#pragma once

struct ObjectData
{
	float4x4 transform;
	float4 sphere;
	uint material_offset;
	uint lod_offset;
	uint pack_bucket_lcount;
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

struct ObjectInstance
{
	uint input_offset;
	uint output_offset;
};

struct ClusterInstance
{
	uint object;
	uint cluster;
};

struct RenderBucket
{
	uint offset;
	uint count;
};

struct IndirectCommand
{
	uint index_count;
	uint instance_count;
	uint index_offset;
	int vertex_offset;
	uint instance_id;
};
