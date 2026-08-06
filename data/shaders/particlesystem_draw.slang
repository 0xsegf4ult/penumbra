#include "particlesystem.slang"
#include "visbuffer_shaderdata.slang"

ConstantBuffer<VisbufferConstants> consts;

struct PSOInput
{
	Ptr<Particle, Access.Read> particles;
	Ptr<uint, Access.Read> alive_list;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float3 world_pos : WORLD_POS;
};

static const float2 billboard[6] = {
	float2(-1.0, -1.0),
	float2(1.0, -1.0),
	float2(-1.0, 1.0),
	float2(-1.0, 1.0),
	float2(1.0, -1.0),
	float2(1.0, 1.0)
};

[shader("vertex")]
VSOutput vertexMain(uniform PSOInput data, uint vertexIndex : SV_VertexID)
{
	VSOutput output;

	Ptr<Particle, Access.Read> p = data.particles + data.alive_list[vertexIndex / 6];
	output.world_pos = p->position + float3(billboard[vertexIndex % 6] * p->size, 0.0);
	output.position = mul(float4(output.world_pos, 1.0), consts.camera);
	return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target0
{
	return float4(0.8, 0.8, 0.8, 1.0);
}
