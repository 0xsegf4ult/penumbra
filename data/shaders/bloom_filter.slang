#include "globals.slang"

struct CSData
{
	ResourceHandle<Texture2D<float3>> input;
	ResourceHandle<RWTexture2D<float3>> output;
	uint2 res;
	float2 inv_res;
	float2 direction;
	int level;
};

static const uint GAUSS_KERNEL = 33;
static const float gaussianWeightsNormalized[GAUSS_KERNEL] = {
	0.004013, 0.005554, 0.007527,
	0.00999, 0.012984, 0.016524,
	0.020594, 0.025133, 0.030036,
	0.035151, 0.040283, 0.045207,
	0.049681, 0.053463, 0.056341,
	0.058141, 0.058754, 0.058141,
	0.056341, 0.053463, 0.049681,
	0.045207, 0.040283, 0.035151,
	0.030036, 0.025133, 0.020594,
	0.016524, 0.012984, 0.00999,
	0.007527, 0.005554, 0.004013
};

static const int gaussianOffsets[GAUSS_KERNEL] = {
	-16, -15, -14,
	-13, -12, -11,
	-10, -9, -8,
	-7, -6, -5,
	-4, -3, -2,
	-1, 0, 1,
	2, 3, 4,
	5, 6, 7,
	8, 9, 10,
	11, 12, 13,
	14, 15, 16
};

static const int TILE_BORDER = GAUSS_KERNEL / 2;
static const int GAUSSIAN_BLUR_THREADCOUNT = 256;
static const int CACHE_SIZE = TILE_BORDER * 2 + GAUSSIAN_BLUR_THREADCOUNT;
groupshared float3 color_cache[CACHE_SIZE];

[shader("compute")]
[numthreads(GAUSSIAN_BLUR_THREADCOUNT, 1, 1)]
void computeMain(uint2 Gid : SV_GroupID, uint groupIndex : SV_GroupIndex, uniform CSData data)
{
	uint2 tile_start = Gid;
	if(data.direction.y == 0.0f)
		tile_start.x *= GAUSSIAN_BLUR_THREADCOUNT;
	else
		tile_start.y *= GAUSSIAN_BLUR_THREADCOUNT;

	ResourceHandle<SamplerState> smp = {SAMPLER_LINEAR_CLAMP};
	for(int i = groupIndex; i < CACHE_SIZE; i += GAUSSIAN_BLUR_THREADCOUNT)
	{
		const float2 uv = (tile_start + 0.5 + data.direction * (i - TILE_BORDER)) * data.inv_res;
		color_cache[i] = data.input.desc->SampleLevel(smp.desc, uv, data.level);
	}

	GroupMemoryBarrierWithGroupSync();

	const uint2 pixel = tile_start + groupIndex * data.direction;
	if(pixel.x >= data.res.x || pixel.y >= data.res.y)
		return;

	const uint center = TILE_BORDER + groupIndex;

	float3 color = 0;
	for(uint i = 0; i < GAUSS_KERNEL; i++)
	{
		const uint s = center + gaussianOffsets[i];
		const float3 color2 = color_cache[s];
		color += color2 * gaussianWeightsNormalized[i];
	}

	data.output.desc[pixel] = color;
}
