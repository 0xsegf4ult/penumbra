#include "globals.slang"
#include "render_world.slang"
#include "visbuffer_shaderdata.slang"
#include "pbr_material.slang"
#include "normal_encoding.slang"

ConstantBuffer<VisbufferConstants> consts;

struct PSOInput
{
	Ptr<ObjectData, Access.Read> objects;
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<MaterialPBR, Access.Read> materials;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float4 wpos : WORLD_POS;	
	float2 uv : UV0;
	float3 normal : NORMAL;
	uint material : MATERIAL;
	uint drawID : DRAW_ID;
};

[shader("vertex")]
VSOutput vertexMain(uniform PSOInput input, uint index : SV_VulkanVertexID, uint instance : SV_VulkanInstanceID)
{
	VSOutput output;
	output.uv = consts.vertex_uv[index];
	output.material = input.objects[input.instances[instance].object - 1].material_offset;
	output.drawID = instance;

	float4x4 trs = input.objects[input.instances[instance].object - 1].transform;
	
	float3x3 trs_invT = float3x3(
		cross(trs[1].xyz, trs[2].xyz),
		cross(trs[2].xyz, trs[0].xyz),
		cross(trs[0].xyz, trs[1].xyz)
	);

	float3 inNormal = oct_to_float3(consts.vertex_nor_tan[index].x);
	output.normal = normalize(mul(inNormal, trs_invT));

	output.wpos = mul(float4(consts.vertex_pos[index], 1.0), trs);
	output.pos = mul(output.wpos, consts.camera);
	return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input, uniform PSOInput data, bool frontfacing : SV_IsFrontFace) : SV_Target0
{
	ResourceHandle<SamplerState> smp = {0u};
	ResourceHandle<SamplerState> smp_aniso = {SAMPLER_ANISO_REPEAT};
	Ptr<MaterialPBR, Access.Read> material = data.materials + WaveReadLaneFirst(input.material);
	
	float4 albedo = (material->albedo.valid() ? nonuniform(material->albedo.desc)->Sample(smp_aniso.desc, input.uv) : float4(1.0)) * material->albedo_factor;
	float3 emissive = (material->emissive.valid() ? nonuniform(material->emissive.desc)->Sample(smp_aniso.desc, input.uv).xyz : float3(1.0)) * material->emissive_factor.xyz;
	float3 physical = material->mro.valid() ? nonuniform(material->mro.desc)->Sample(smp_aniso.desc, input.uv).xyz : float3(1.0, 1.0, 0.0);

	float3 N = normalize(input.normal);
	if(!frontfacing)
		N = -N;
	
	float3 V = normalize(consts.cam_pos.xyz - input.wpos.xyz);

	float roughness = physical.r * material->roughness_factor;
	float metallic = physical.g * material->metallic_factor;
	
	float NdotV = dot(N, V);
	N += (2.0 * saturate(-NdotV)) * V;

	float3 amb_IRR = consts.irradiance.desc->SampleLevel(smp.desc, N, 0) * consts.ambient_factor;

	NdotV = dot(N, V);

	float3 l0 = float3(0.0);
	float reflectance = 0.16 * material->reflectivity * material->reflectivity;
	float3 diffuse = ((1.0 - metallic) * albedo.xyz);	
	float3 f0 = float3(reflectance * (1.0 - metallic)) + albedo.xyz * metallic;
	float3 f90 = saturate(50.0f * f0);

	ResourceHandle<SamplerState> smp_clamp = {SAMPLER_LINEAR_CLAMP};
	float2 envBRDF = consts.brdf.desc->SampleLevel(smp_clamp.desc, float2(max(NdotV, 0.0), roughness), 0);

	float3 reflection = reflect(-V, N);
	float3 amb_PFC = consts.prefiltered.desc->SampleLevel(smp.desc, reflection, roughness * 8.0) * consts.ambient_factor;

	roughness = max(roughness, 0.045);

	float horizon = min(1.0 + dot(reflection, N), 1.0);
	float3 amb_SPEC = amb_PFC * (f0 * envBRDF.x + f90 * envBRDF.y) * horizon * horizon;
	float3 ambient = diffuse * amb_IRR + amb_SPEC;

	return float4((ambient + l0 + emissive) * consts.exposure * albedo.a, albedo.a);
}
