#include "globals.slang"
#include "render_world.slang"
#include "visbuffer_shaderdata.slang"
#include "pbr_material.slang"

ConstantBuffer<VisbufferConstants> consts;

struct VSInput
{
	Ptr<ObjectData, Access.Read> objects;
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<MaterialPBR, Access.Read> materials;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv : UV0;
	uint material : MATERIAL;
	uint drawID : DRAW_ID;
};

[shader("vertex")]
VSOutput vertexMain(uniform VSInput input, uint index : SV_VulkanVertexID, uint instance : SV_VulkanInstanceID)
{
	VSOutput output;
	output.uv = consts.vertex_uv[index];
	output.material = input.objects[input.instances[instance].object - 1].material_offset;
	output.drawID = instance;

	float4x4 trs = input.objects[input.instances[instance].object - 1].transform;
	float4 outWorldPos = mul(float4(consts.vertex_pos[index], 1.0), trs);
	output.pos = mul(outWorldPos, consts.camera);
	return output;
}

[shader("fragment")]
uint fragmentMain(VSOutput input, uniform VSInput data, uint primitive : SV_PrimitiveID) : SV_Target0
{
	ResourceHandle<SamplerState> smp_aniso = {1u};
	uint mtl_offset = WaveReadLaneFirst(input.material);
	Ptr<MaterialPBR, Access.Read> material = data.materials + mtl_offset;
	float alpha = material->albedo.desc.Sample(smp_aniso.desc, input.uv).a;
	if(alpha < material->alpha_cf)
		discard;

	return ((input.drawID + 1u) << 7) | (primitive & 0x7F);
}
