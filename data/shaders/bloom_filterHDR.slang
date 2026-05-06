#include "globals.slang"

struct CSData
{
	ResourceHandle<Texture2D<float3>> input;
	ResourceHandle<RWTexture2D<float3>> output;
	float2 inv_res;
	float threshold;
	float max_value;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint2 DTid : SV_DispatchThreadID, uniform CSData data)
{
	ResourceHandle<SamplerState> smp = {SAMPLER_LINEAR_CLAMP};
	
	float3 color = float3(0.0);
	const float2 uv = DTid + 0.5;

	color += data.input.desc.SampleLevel(smp.desc, (uv + float2(-0.5, -0.5)) * data.inv_res, 0);
	color += data.input.desc.SampleLevel(smp.desc, (uv + float2(0.5, -0.5)) * data.inv_res, 0);
	color += data.input.desc.SampleLevel(smp.desc, (uv + float2(-0.5, 0.5)) * data.inv_res, 0);
	color += data.input.desc.SampleLevel(smp.desc, (uv + float2(0.5, 0.5)) * data.inv_res, 0);

	color *= 0.25;
	color = min(color, data.max_value);
	
	float3 s2 = saturate(color - (data.threshold * 0.5));
	color = max(color - data.threshold, s2 * s2 * 0.5);

	data.output.desc[DTid] = color;
}
