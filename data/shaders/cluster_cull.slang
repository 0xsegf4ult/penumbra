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

bool is_visible(float4 center, float radius, float3 cone, float cone_cutoff)
{
	bool visible = true;

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
	float radius = cluster->sphere.w;
	float3 cone_axis = mul(cluster.cone.xyz, float3x3(object->transform));

	if(is_visible(center, radius, cone_axis, cluster.cone.w))
	{
		uint bucket = object->pack_bucket_lcount >> 16;
		InterlockedAdd(input.buckets[bucket].count, 1u);
		input.visibility[DTid] = 1;
	}	
}
