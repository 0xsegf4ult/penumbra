#include "visbuffer_shaderdata.slang"

ConstantBuffer<VisbufferConstants> consts;

float point_aabb_distsqr(float3 point, uint cluster)
{
	float dist = 0.0;
	Ptr<LightCluster, Access.ReadWrite> cur_cell = consts.light_clusters + cluster;
	for(int i = 0; i < 3; i++)
	{
		float v = point[i];
		if(v < cur_cell.min[i])
			dist += (cur_cell.min[i] - v) * (cur_cell.min[i] - v);

		if(v > cur_cell.max[i])
			dist += (v - cur_cell.max[i]) * (v - cur_cell.max[i]);
	}

	return dist;
}

bool test_sphere_aabb(float3 center, float radius, uint cluster)
{
	return point_aabb_distsqr(center, cluster) <= (radius * radius);
}

groupshared Light smem_lights[LIGHT_CLUSTERS_X * LIGHT_CLUSTERS_Y * LIGHT_CLUSTERS_Z];

[shader("compute")]
[numthreads(LIGHT_CLUSTERS_X, LIGHT_CLUSTERS_Y, LIGHT_CLUSTERS_Z)]
void computeMain(uint groupIndex : SV_GroupIndex, uint3 Gid : SV_GroupID)
{
	if(groupIndex == 0)
	{
		consts.light_counters[0] = 0;
		consts.light_counters[1] = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	const uint threadCount = LIGHT_CLUSTERS_X * LIGHT_CLUSTERS_Y * LIGHT_CLUSTERS_Z;
	const uint numPLBatches = (consts.point_light_count + threadCount - 1) / threadCount;
	const uint numSLBatches = (consts.spot_light_count + threadCount - 1) / threadCount;

	const uint clusterIndex = groupIndex + threadCount * Gid.z;

	uint visiblePointLightCount = 0;
	uint visiblePointLightIndices[64];

	for(uint batch = 0; batch < numPLBatches; batch++)
	{
		uint lightIndex = batch * threadCount + groupIndex;
		lightIndex = min(lightIndex, 1023);
		
		smem_lights[groupIndex] = consts.point_lights[lightIndex];
		smem_lights[groupIndex].pos_ic = mul(float4(smem_lights[groupIndex].pos_ic.xyz, 1.0), consts.view);
		GroupMemoryBarrierWithGroupSync();

		for(uint light = 0; light < threadCount; light++)
		{
			if(test_sphere_aabb(smem_lights[light].pos_ic.xyz, smem_lights[light].color_rad.w, clusterIndex))
			{
				visiblePointLightIndices[visiblePointLightCount] = batch * threadCount + light;
				visiblePointLightCount++;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	uint visibleSpotlightCount = 0;
	uint visibleSpotlightIndices[64];

	for(uint batch = 0; batch < numSLBatches; batch++)
        {
                uint lightIndex = batch * threadCount + groupIndex;
                lightIndex = min(lightIndex, 1023);

                smem_lights[groupIndex] = consts.spot_lights[lightIndex];
                smem_lights[groupIndex].pos_ic = mul(float4(smem_lights[groupIndex].pos_ic.xyz, 1.0), consts.view);
                GroupMemoryBarrierWithGroupSync();

                for(uint light = 0; light < threadCount; light++)
                {
                        if(test_sphere_aabb(smem_lights[light].pos_ic.xyz, smem_lights[light].color_rad.w, clusterIndex))
                        {
                                visibleSpotlightIndices[visibleSpotlightCount] = batch * threadCount +
 light;
                                visibleSpotlightCount++;
                        }
                }
        }

	GroupMemoryBarrierWithGroupSync();

	uint pl_offset;
	InterlockedAdd(consts.light_counters[0], visiblePointLightCount, pl_offset);
	uint sl_offset;
	InterlockedAdd(consts.light_counters[1], visibleSpotlightCount, sl_offset);

	for(uint i = 0; i < visiblePointLightCount; i++)
	{
		consts.point_light_indices[pl_offset + i] = visiblePointLightIndices[i];
	}

	consts.point_light_grid[clusterIndex].offset = pl_offset;
	consts.point_light_grid[clusterIndex].count = visiblePointLightCount;

	for(uint i = 0; i < visibleSpotlightCount; i++)
	{
		consts.spot_light_indices[sl_offset + i] = visibleSpotlightIndices[i];
	}

	consts.spot_light_grid[clusterIndex].offset = sl_offset;
	consts.spot_light_grid[clusterIndex].count = visibleSpotlightCount;
}
