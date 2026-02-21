#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<ObjectInstance, Access.Read> instances;
	Ptr<ClusterInstance, Access.ReadWrite> clusters;
	Ptr<RenderBucket, Access.Read> buckets;
	Ptr<ObjectData, Access.Read> objects;
	Ptr<GeometryLOD, Access.Read> lods;
	uint count;
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
[numthreads(32, 1, 1)]
void computeMain(uint DTid: SV_DispatchThreadID, uniform CSInput input)
{
	if(DTid >= input.count)
		return;

	ObjectInstance instance = input.instances[DTid];
	if(instance.input_offset == 0)
		return;

	Ptr<ObjectData, Access.Read> object = input.objects + (instance.input_offset - 1);
	float4 center = mul(float4(object->sphere.xyz, 1.0), object->transform);
	float radius = object->sphere.w;

	uint lod_count = object->pack_bucket_lcount & 0xFFFF;
	uint bucket = object->pack_bucket_lcount >> 16;
	uint global_offset = input.buckets[bucket].offset;

	float3 cam_pos = -culling_data.viewmat[3].xyz;
	float dist = distance(center.xyz, cam_pos);
	uint lodIndex = uint(max(1.0 + log(dist / culling_data.lod_base) / log(culling_data.lod_step), 0.0));
	lodIndex = min(lodIndex, lod_count - 1);

	GeometryLOD lod = input.lods[object->lod_offset + lodIndex];

	if(is_visible(center, radius))
	{
		for(uint i = 0; i < lod.cluster_count; i++)
		{
			input.clusters[global_offset + instance.output_offset + i].object = instance.input_offset;
			input.clusters[global_offset + instance.output_offset + i].cluster = lod.cluster_offset + i;
		}
	}
}
