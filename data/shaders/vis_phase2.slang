#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<GPUWorldRenderable, Access.Read> renderables;
	Ptr<GeometryCluster, Access.Read> geom_clusters;
	Ptr<VisCounters, Access.ReadWrite> counters;
	Ptr<GPUWorldMeshlet, Access.Read> meshlets;
	Ptr<GPUWorldMeshletInstance, Access.ReadWrite> instances;
	Ptr<uint, Access.Read> meshlet_sums;
	Ptr<GPUWorldBucket, Access.ReadWrite> buckets;
	Ptr<DrawIndexedIndirectCommand, Access.ReadWrite> indirect_args;
};

ConstantBuffer<CullingData> culling_data;

bool is_visible(float4 sphere, float radius, float3 cone, float cone_cutoff)
{
	bool visible = true;

	float3 center = mul(sphere, culling_data.viewmat).xyz;
	float3 cone_axis = mul(cone, float3x3(culling_data.viewmat));

	if(culling_data.flags & CullingFlags.FrustumCull)
	{
		if(culling_data.flags & CullingFlags.IsOrtho)
		{
			for(int i = 0; i < 4; i++)
				visible = visible && dot(culling_data.frustum_planes[i], float4(center, 1.0)) > -radius;
		}
		else
		{
			visible = visible && center.z * culling_data.frustum_planes[0].y - abs(center.x) * culling_data.frustum_planes[0].x > -radius;
			visible = visible && center.z * culling_data.frustum_planes[0].w - abs(center.y) * culling_data.frustum_planes[0].z > -radius;
			visible = visible && -center.z + radius > culling_data.znear && -center.z - radius < culling_data.zfar;
		}
	}

	if(visible && culling_data.flags & CullingFlags.ConeCull)
	{
		if(culling_data.flags & CullingFlags.IsOrtho)
			visible = visible && dot(center, cone_axis) < cone_cutoff;
		else
			visible = visible && dot(center, cone_axis) < cone_cutoff * length(center) + radius;
	}

	return visible;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint DTid: SV_DispatchThreadID, uint Gidx : SV_GroupIndex, uniform CSInput input)
{
	if(DTid < input.counters.visible_meshlets)
	{
		Ptr<GPUWorldMeshlet, Access.Read> meshlet = input.meshlets + DTid;

		Ptr<GPUWorldRenderable, Access.Read> renderable = input.renderables + (meshlet->renderable_index);
		Ptr<GeometryCluster, Access.Read> cluster = input.geom_clusters + meshlet->meshlet_index;
		float4 center = mul(float4(cluster->sphere.xyz, 1.0), renderable->transform);
		float radius = cluster->sphere.w * renderable->cull_scale;
		float3 cone_axis = mul(cluster->cone.xyz, float3x3(renderable->transform));

		if(is_visible(center, radius, cone_axis, cluster->cone.w))
		{
			uint idx;
			InterlockedAdd(input.buckets[meshlet.bucket].counter, 1u, idx);
/*
			InterlockedAdd(input.indirect_args[meshlet.bucket].instance_count, 1u, idx);
			if(idx == 0)
			{
				input.indirect_args[meshlet.bucket].first_instance = input.meshlet_sums[meshlet.bucket];
			}

			InterlockedMax(input.indirect_args[meshlet.bucket].vertex_count, cluster.vertex_count);
*/
			Ptr<GPUWorldMeshletInstance, Access.ReadWrite> instance = input.instances + (input.meshlet_sums[meshlet.bucket] + idx);
			instance->renderable_index = meshlet->renderable_index;
			instance->meshlet_index = meshlet->meshlet_index;

			Ptr<DrawIndexedIndirectCommand, Access.ReadWrite> command = input.indirect_args + (input.buckets[meshlet.bucket].offset + idx);	
			command->index_count = cluster->index_count;
			command->instance_count = 1u;
			command->first_index = renderable->geom_idx_offset + cluster->index_offset;
			command->first_vertex = renderable->geom_vtx_offset + cluster->vertex_offset;
			command->first_instance = input.meshlet_sums[meshlet.bucket] + idx;
		}
	}	

	AllMemoryBarrierWithGroupSync();

	if(Gidx == 0)
	{
		uint tgid;
		InterlockedAdd(input.counters.threadgroup_count, 1u, tgid);
		uint tgcount = (input.counters.visible_meshlets + 63) / 64;
		bool last_thr = (tgid + 1 == tgcount);
	}	
}
