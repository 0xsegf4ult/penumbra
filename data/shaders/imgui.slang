#include "globals.slang"

struct ImGUIVertex
{
	float2 pos;
	float2 uv;
	uint color;
};

struct VSInput
{
	Ptr<ImGUIVertex, Access.Read> vertices;
	ResourceHandle<Texture2D> tex;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv : UV0;
	float4 color : COLOR;
};

ConstantBuffer<float4x4> proj;

[shader("vertex")]
VSOutput vertexMain(uint vertex : SV_VulkanVertexID, uniform VSInput input)
{
	ImGUIVertex vert = input.vertices[vertex];

	VSOutput output;
	output.uv = vert.uv;

	float4 color = unpack_rgba(vert.color);
	output.color = float4(remove_srgb_curve(color.rgb), color.a);
	output.pos = mul(float4(vert.pos, 0.0, 1.0), proj);
	return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input, uniform VSInput consts) : SV_Target0
{
	ResourceHandle<SamplerState> smp = {0u};
	return input.color * consts.tex.desc.Sample(smp.desc, input.uv);
}
