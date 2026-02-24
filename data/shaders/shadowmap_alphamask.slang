#include "globals.slang"
#include "render_world.slang"
#include "pbr_material.slang"

struct VSInput
{
	Ptr<ObjectData, Access.Read> objects;
	Ptr<MaterialPBR, Access.Read> materials;
	Ptr<float3, Access.Read> vertex_pos;
	Ptr<float2, Access.Read> vertex_uv;
	Ptr<float4x4, Access.Read> smap_data;
	uint smap_index;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float2 uv : UV0;
	uint material : MATERIAL;
};

[shader("vertex")]
VSOutput vertexMain(uniform VSInput input, uint index : SV_VulkanVertexID, uint instance : SV_VulkanInstanceID)
{
	VSOutput output;
	output.uv = input.vertex_uv[index];
	output.material = input.objects[instance].material_offset;

	float4x4 trs = input.objects[instance].transform;
	float4 outWorldPos = mul(float4(input.vertex_pos[index], 1.0), trs);
	output.pos = mul(outWorldPos, input.smap_data[input.smap_index]);
	return output;
}

[shader("fragment")]
void fragmentMain(VSOutput input, uniform VSInput data)
{
	Ptr<MaterialPBR, Access.Read> mtl = data.materials + WaveReadLaneFirst(input.material);

	ResourceHandle<SamplerState> smp_aniso = {1u};
	float alpha = nonuniform(mtl->albedo.desc)->Sample(smp_aniso.desc, input.uv).a;
	if(alpha < mtl->alpha_cf)
		discard;
}
