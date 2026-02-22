#include "ibl_common.slang"

struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
	VSOutput output;
	output.uv.x = vertexID << 1 & 2;
	output.uv.y = vertexID & 2;
	output.pos = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
	return output;
}

static const uint numSamples = 1024u;

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target0
{
	const float NoV = input.uv.x;
	const float roughness = input.uv.y;
	
	const float3 N = float3(0.0, 0.0, 1.0);
	const float3 V = float3(sqrt(1.0 - NoV * NoV), 0.0, NoV);

	float2 LUT = float2(0.0);
	for(uint i = 0; i < numSamples; i++)
	{
		const float2 Xi = Hammersley2D(i, numSamples);
		const float3 H = ImportanceSample_GGX(Xi, roughness, N);
		const float3 L = 2.0 * dot(V, H) * H - V;
	
		const float dotNL = max(dot(N, L), 0.0);
		const float dotNV = max(dot(N, V), 0.0);
		const float dotVH = max(dot(V, H), 0.0);
		const float dotNH = max(dot(H, N), 0.0);

		if(dotNL > 0.0)
		{
			const float G = GeometrySchlickSmithGGX(dotNL, dotNV, roughness);
			const float G_Vis = (G * dotVH) / (dotNH * dotNV);
			const float Fc = pow(1.0 - dotVH, 5.0);
			LUT += float2((1.0 - Fc) * G_Vis, Fc * G_Vis);
		}
	}

	return float4(LUT / float(numSamples), 0.0, 1.0);
}
