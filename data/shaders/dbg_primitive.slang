#include "globals.slang"
#include "visbuffer_shaderdata.slang"

ConstantBuffer<VisbufferConstants> consts;

struct ShaderData
{
	Ptr<float3, Access.Read> vertices;
	Ptr<float4, Access.Read> color;
	uint prim_size;	
};

struct VSOutput
{
	float4 pos : SV_Position;
	float3 color : COLOR;
	float size : SV_PointSize;
};

[shader("vertex")]
VSOutput vertexMain(uint vertex : SV_VulkanVertexID, uint instance : SV_VulkanInstanceID, uniform ShaderData data)
{
	float3 vert = data.vertices[vertex + instance * data.prim_size];

	VSOutput output;

	output.size = 10.0f;
	output.color = data.color[instance].xyz * data.color[instance].w;
	output.pos = mul(float4(vert, 1.0), consts.camera);
	return output;
}

[shader("fragment")]
float4 fragmentMain(float3 color : COLOR) : SV_Target0
{
	return float4(color, 1.0);
}
