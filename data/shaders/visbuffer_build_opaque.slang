#include "globals.slang"
#include "render_world.slang"

struct Camera
{
	float4x4 matrix;
};

ConstantBuffer<Camera> camera;

struct VSInput
{
	Ptr<float3, Access.Read> vertex_pos;
	Ptr<ObjectData, Access.Read> objects;
	Ptr<ClusterInstance, Access.Read> instances;
};

struct VSOutput
{
	float4 pos : SV_Position;
	uint drawID : DRAW_ID;
};

[shader("vertex")]
VSOutput vertexMain(uniform VSInput input, uint index : SV_VulkanVertexID, uint instance : SV_VulkanInstanceID)
{
	VSOutput output;
	output.drawID = instance;

	float4x4 trs = input.objects[input.instances[instance].object - 1].transform;
	float4 outWorldPos = mul(float4(input.vertex_pos[index], 1.0), trs);
	output.pos = mul(outWorldPos, camera.matrix);
	return output;
}

[shader("fragment")]
[earlydepthstencil]
uint fragmentMain(VSOutput input, uint primitive : SV_PrimitiveID) : SV_Target0
{
	return ((input.drawID + 1u) << 7) | (primitive & 0x7F);
}
