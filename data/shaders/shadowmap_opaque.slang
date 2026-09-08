#include "render_world.slang"

struct VSInput
{
	Ptr<GPUWorldRenderable, Access.Read> renderables;
	Ptr<GPUWorldMeshletInstance, Access.Read> instances;
	Ptr<float3, Access.Read> vertex_pos;
	Ptr<float4x4, Access.Read> smap_data;
	uint smap_index;
};

[shader("vertex")]
float4 vertexMain(uniform VSInput input, uint index : SV_VulkanVertexID, uint instance : SV_VulkanInstanceID) : SV_Position
{
	float4x4 trs = input.renderables[input.instances[instance].renderable_index].transform;
	float4 outWorldPos = mul(float4(input.vertex_pos[index], 1.0), trs);
	return mul(outWorldPos, input.smap_data[input.smap_index]);
}
