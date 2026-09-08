#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<GPUWorldRenderable, Access.Read> renderables;
	Ptr<GeometryLOD, Access.Read> geom_lods;
	Ptr<VisCounters, Access.ReadWrite> counters;
	Ptr<GPUWorldMeshlet, Access.ReadWrite> visible_meshlets;
	Ptr<uint, Access.ReadWrite> meshlet_sums;
	Ptr<uint3, Access.ReadWrite> dispatch_args;
};

ConstantBuffer<CullingData> culling_data;

bool is_visible(float4 sphere, float radius)
{
	bool visible = true;
	float3 center = mul(sphere, culling_data.viewmat).xyz;

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

	return visible;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint DTid : SV_DispatchThreadID, uint Gidx : SV_GroupIndex, uniform CSInput input)
{
	bool visible = (DTid < input.counters.renderable_count);

	Ptr<GPUWorldRenderable, Access.Read> renderable = input.renderables + DTid;
	if(visible)
		visible = !(renderable->flags & ObjectFlags.Disabled);

	float4 center;
	if(visible)
	{
		center = mul(float4(renderable->sphere.xyz, 1.0), renderable->transform);
		float radius = renderable->sphere.w * renderable->cull_scale;
		
		visible = is_visible(center, radius);
	}

	if(visible)
	{
		uint lod_count = renderable->pack_bucket_lcount & 0xFFFF;
		float dist = distance(center.xyz, culling_data.cam_pos.xyz);
		uint lodIndex = uint(max(1.0 + log(dist / culling_data.lod_base) / log(culling_data.lod_step), 0.0));
		lodIndex = min(lodIndex + culling_data.lod_bias, lod_count - 1);

		GeometryLOD lod = input.geom_lods[renderable->lod_offset + lodIndex];

		GPUWorldMeshlet meshlet;
		meshlet.renderable_index = DTid;
		meshlet.bucket = renderable->pack_bucket_lcount >> 16;
		meshlet.lod = lodIndex;
		meshlet.meshlet_index = renderable->geom_cluster_offset + lod.cluster_offset;

		uint first_meshlet;
		InterlockedAdd(input.counters.visible_meshlets, lod.cluster_count, first_meshlet);
		
		for(uint i = 0; i < lod.cluster_count; i++)
		{
			input.visible_meshlets[first_meshlet + i] = meshlet;
			++meshlet.meshlet_index;
		}

		InterlockedAdd(input.meshlet_sums[(renderable->pack_bucket_lcount >> 16) + 1], lod.cluster_count);
	}

	AllMemoryBarrierWithGroupSync();

	if(Gidx == 0)
	{
		uint tgid;
		InterlockedAdd(input.counters.threadgroup_count, 1u, tgid);
		uint tgcount = (input.counters.renderable_count + 63) / 64;
		bool last_thr = (tgid + 1 == tgcount);

		if(last_thr)
		{
			input.dispatch_args[0].x = (input.counters.visible_meshlets + 63) / 64;
			input.dispatch_args[0].y = 1;
			input.dispatch_args[0].z = 1;
		
			uint prev_bcount = 0;
			for(uint i = 1; i < 6; i++)
			{
				uint old;
				InterlockedAdd(input.meshlet_sums[i], prev_bcount, old);
				prev_bcount += old;
			}

			input.counters.threadgroup_count = 0;
		}
	}
}
