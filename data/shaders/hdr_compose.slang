#include "globals.slang"

struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv;
};

struct FSInput
{
	ResourceHandle<Texture2D> hdrbuffer;
	int tonemapper;
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

float3 applyACES(float3 color)
{
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;

	return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
} 

float3 applyAGX(float3 color)
{
	static const float3x3 agx_matrix_inset =
	{
		{ 0.8566271533159880, 0.1373185106779920, 0.1118982129999500 },
		{ 0.0951212405381588, 0.7612419900249430, 0.0767997842235547 },
		{ 0.0482516061458523, 0.1014394992970650, 0.8113020027764950 }
	};

	static const float3x3 agx_matrix_outset =
	{
		{ 1.1271467782272380, -0.1468813165635330, -0.1255038609319300 },
		{ -0.0496500000000000, 1.1084784877776500, -0.0524964871144260 },
		{ -0.0774967775043101, -0.1468813165635330, 1.2244001486462500 }
	};

	const float agx_min_ev = -10.0f;
	const float agx_max_ev = 6.5f;

	color = mul(color, agx_matrix_inset);
	color = max(color, 1e-10);
	color = log2(color);
	color = (color - agx_min_ev) / (agx_max_ev - agx_min_ev);
	color = saturate(color);

	float3 x = color;
	float3 x2 = color * color;
	float3 x4 = x2 * x2;
	color = 15.5f * x4 * x2 - 40.14f * x4 * x + 31.96f * x4 - 6.868f * x2 * x + 0.4298f * x2 + 0.1191f * x - 0.00232f;

	float3 lw = float3(0.2126, 0.7152, 0.0722);
	float luma = dot(color, lw);
	float3 offset = color - luma;
	float saturation = 1.35f;
	float contrast = 1.10f;
	float contrast_mid = 0.18f;
	color = luma + offset * saturation;
	color = contrast_mid + (color - contrast_mid) * contrast;
	color = saturate(color);

	color = mul(color, agx_matrix_outset);
	return saturate(color);
}

[shader("fragment")]
float4 fragmentMain(VSOutput input, uniform FSInput data) : SV_Target0
{	
	ResourceHandle<SamplerState> smp = {0u};
	float3 hdr_color = data.hdrbuffer.desc.SampleLevel(smp.desc, input.uv, 0).xyz;

	switch(data.tonemapper)
	{
	case 0:
		hdr_color = applyACES(hdr_color);
		break;
	case 1:
		hdr_color = applyAGX(hdr_color);
		break;
	default:
		break;
	}
	
	return float4(hdr_color, 1.0);
}
