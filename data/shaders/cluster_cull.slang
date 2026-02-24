#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<uint, Access.ReadWrite> visibility;	
	Ptr<RenderBucket, Access.ReadWrite> buckets;
	Ptr<ObjectData, Access.Read> objects;
	Ptr<GeometryCluster, Access.Read> clusters;
	uint count;
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
[numthreads(32, 1, 1)]
void computeMain(uint DTid: SV_DispatchThreadID, uniform CSInput input)
{
	if(DTid >= input.count)
		return;

	ClusterInstance instance = input.instances[DTid];
	if(instance.object == 0)
		return;

	Ptr<ObjectData, Access.Read> object = input.objects + (instance.object - 1);
	Ptr<GeometryCluster, Access.Read> cluster = input.clusters + instance.cluster;
	float4 center = mul(float4(cluster->sphere.xyz, 1.0), object->transform);
	float radius = cluster->sphere.w * object->cull_scale;
	float3 cone_axis = mul(cluster->cone.xyz, float3x3(object->transform));

	if(is_visible(center, radius, cone_axis, cluster->cone.w))
	{
		uint bucket = object->pack_bucket_lcount >> 16;
		InterlockedAdd(input.buckets[bucket].count, 1u);
		input.visibility[DTid] = 1;
	}	
}
