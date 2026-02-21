#include "globals.slang"

struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv;
};

struct FSInput
{
	ResourceHandle<Texture2D> hdrbuffer;
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

[shader("fragment")]
float4 fragmentMain(VSOutput input, uniform FSInput data) : SV_Target0
{	
	ResourceHandle<SamplerState> smp = {0u};
	float3 hdr_color = data.hdrbuffer.desc.SampleLevel(smp.desc, input.uv, 0).xyz;

	float3 mapped = applyACES(hdr_color);
	return float4(mapped, 1.0);
}
