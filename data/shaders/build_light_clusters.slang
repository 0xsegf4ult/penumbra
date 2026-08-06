#include "visbuffer_shaderdata.slang"

ConstantBuffer<VisbufferConstants> consts;

float4 clip_to_view(float4 clip)
{
	float4 view = mul(clip, consts.inverse_projection);
	return view / view.w;
}

float4 screen_to_view(float4 screen)
{
	float2 texCoord = screen.xy * consts.inv_res;
	float4 clip = float4(texCoord * 2.0 - 1.0, screen.z, screen.w);
	return clip_to_view(clip);
}

[shader("compute")]
[numthreads(LIGHT_CLUSTERS_X, LIGHT_CLUSTERS_Y, LIGHT_CLUSTERS_Z)]
void computeMain(uint3 DTid : SV_DispatchThreadID)
{
	uint clusterIndex = DTid.z * LIGHT_CLUSTERS_X * LIGHT_CLUSTERS_Y +
			    DTid.y * LIGHT_CLUSTERS_X + 
			    DTid.x;

	float4 minPoint_scr = float4(DTid.xy * consts.cluster_sizes.xy, 1.0, 1.0);
	float4 maxPoint_scr = float4(float2(DTid.x + 1, DTid.y + 1) * consts.cluster_sizes.xy, 1.0, 1.0);

	float3 minPoint_view = screen_to_view(minPoint_scr).xyz;
	float3 maxPoint_view = screen_to_view(maxPoint_scr).xyz;

	float clusterNear = -consts.znear * pow(consts.zfar / consts.znear, DTid.z / LIGHT_CLUSTERS_Z_ALL);
	float clusterFar = -consts.znear * pow(consts.zfar / consts.znear, (DTid.z + 1) / LIGHT_CLUSTERS_Z_ALL);

	float3 minPointNear = minPoint_view * clusterNear / minPoint_view.z;
	float3 minPointFar = minPoint_view * clusterFar / minPoint_view.z;
	float3 maxPointNear = maxPoint_view * clusterNear / maxPoint_view.z;
	float3 maxPointFar = maxPoint_view * clusterFar / maxPoint_view.z;

	float3 minPointAABB = min(min(minPointNear, minPointFar), min(maxPointNear, maxPointFar));
	float3 maxPointAABB = max(max(minPointNear, minPointFar), max(maxPointNear, maxPointFar));
	
	consts.light_clusters[clusterIndex].min = float4(minPointAABB, 0.0);
	consts.light_clusters[clusterIndex].max = float4(maxPointAABB, 0.0);
}
