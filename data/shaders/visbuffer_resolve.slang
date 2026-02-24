#include "globals.slang"
#include "render_world.slang"
#include "normal_encoding.slang"
#include "pbr_material.slang"
#include "visbuffer_barycentric.slang"
#include "visbuffer_shaderdata.slang"

ConstantBuffer<VisbufferConstants> consts;

struct CSInput
{
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<ObjectData, Access.Read> objects;
	Ptr<MaterialPBR, Access.Read> materials;
	Ptr<GeometryCluster, Access.Read> clusters;
	ResourceHandle<Texture2D<uint>> visbuffer;
	ResourceHandle<RWTexture2D<float3>> framebuffer;
};

float4 clip_to_view(float4 clip)
{
	float4 view = mul(clip, consts.inverse_projection);
	return view / view.w;
}

float4 screen_to_view(float4 screen)
{
	float2 texCoord = screen.xy * consts.inv_res;
	float4 clip = float4(float2(texCoord.x, 1.0 - texCoord.y) * 2.0 - 1.0, screen.z, screen.w);
	return clip_to_view(clip);
}

float3 decompress_bc5n(float2 bc5n)
{
	return float3(bc5n, sqrt(max(1.0 - dot(bc5n, bc5n), 0.0)));
}

static const float2 poissonDisk[16] = {
        float2(-0.94201624, -0.39906216),
        float2(0.94558609, -0.76890725),
        float2(-0.094184101, -0.92938870),
        float2(0.34495938, 0.29387760),
        float2(-0.91588581, 0.45771432),
        float2(-0.81544232, -0.87912464),
        float2(-0.38277543, 0.27676845),
        float2(0.97484398, 0.75648379),
        float2(0.44323325, -0.97511554),
        float2(0.53742981, -0.47373420),
        float2(-0.26496911, -0.41893023),
        float2(-0.79197514, 0.19090188),
        float2(-0.24188840, 0.99706507),
        float2(-0.81409955, 0.91437590),
        float2(0.19984126, 0.78641367),
        float2(0.14383161, -0.14100790)
};

float filterPCF(uint sm_index, float4 posLS)
{
	ResourceHandle<SamplerComparisonState> sm_shadow = {2u};
	float dx = consts.csm_scale * (0.25 * (3 - sm_index));
	float factor = 0.0;

	float3 ndc = posLS.xyz / posLS.w;
	float2 textureUV = float2(ndc.x, ndc.y) * 0.5 + 0.5;
	
	for(int i = 0; i < 16; i++)
	{
		factor += consts.cascade_rts[sm_index].desc.SampleCmpLevelZero(sm_shadow.desc, textureUV + poissonDisk[i] * float2(dx, dx), ndc.z - consts.csm_cbias);
	}

	return factor * 0.0625;
}

float3 calculate_direct_lighting(float3 N, float3 V, float NdotV, float3 diffuse, float roughness, float3 f0, float3 wpos, float3 in_normal, float depth)
{
	float shadow = 1.0;
	uint cascadeIndex = 0;
	for(uint i = 0; i < 2; i++)
	{
		if(depth < consts.cascade_splits[i])
			cascadeIndex = i + 1;
	}

	float3 L = normalize(-consts.light_direction.xyz);
	float NdotL = saturate(dot(N, L));
	float4 sm_samplePos = float4(wpos, 1.0) + (float4(in_normal, 0.0) * dot(in_normal, L) * consts.csm_nbias);
	if(NdotL > 0.01)
		shadow = lerp(0.0, 1.0, filterPCF(cascadeIndex, mul(sm_samplePos, consts.smap_data[cascadeIndex])));

	float3 H = normalize(V + L);
	float NdotH = dot(N, H);
	
	float NDF = DistributionGGX(max(NdotH, 0.0), roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	float3 F = fresnelSchlick(saturate(dot(H, V)), f0);
	
	float denom = rcp(4.0 * max(NdotV, 0.0) * NdotL + 0.0001);
	float3 specular_contrib = NDF * G * F * denom;

	float3 luminance = consts.light_color.rgb * NdotL;
	float3 l0 = (diffuse + specular_contrib) * luminance;
	return shadow * l0;
} 

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint groupIndex : SV_GroupIndex, uint2 Gid : SV_GroupID, uniform CSInput input)
{
	const uint2 invocation = remap_lane_8x8(groupIndex);
	const uint2 pixel = Gid.xy * 8 + invocation.xy;

	uint primitiveID = input.visbuffer.desc[pixel];
	float2 svpos = uint2(pixel.x, consts.res.y - 1 - pixel.y) + float2(0.5);
	float3 ray_origin = mul(screen_to_view(float4(svpos, 1.0, 1.0)), consts.inverse_view).xyz;
	float3 far = mul(screen_to_view(float4(svpos, 1e-6, 1.0)), consts.inverse_view).xyz;
	float3 ray_direction = normalize(far - ray_origin);
	float3 far_dx = mul(screen_to_view(float4(svpos + float2(1.0, 0.0), 1e-6, 1.0)), consts.inverse_view).xyz;
	float3 far_dy = mul(screen_to_view(float4(svpos + float2(0.0, 1.0), 1e-6, 1.0)), consts.inverse_view).xyz;
	float3 ray_direction_quad_x = normalize(far_dx - ray_origin);
	float3 ray_direction_quad_y = normalize(far_dy - ray_origin);

	ResourceHandle<SamplerState> smp = {0u};
	if(primitiveID == 0)
	{
		input.framebuffer.desc[pixel] = consts.prefiltered.desc.SampleLevel(smp.desc, ray_direction, 0) * consts.exposure * consts.ambient_factor;
	}

	float3 p0;
	float3 p1;
	float3 p2;
	float2 uv0;
	float2 uv1;
	float2 uv2;
	uint2 vn0;
	uint2 vn1;
	uint2 vn2;
	float3x3 trs_invT;
	uint material;

	if(!(pixel.x >= consts.res.x || pixel.y >= consts.res.y || primitiveID == 0))
	{

	{
		uint triangleID = primitiveID & 0x7F;
                uint meshletID = (primitiveID >> 7) - 1;

                uint instance = input.instances[meshletID].object - 1;
		float4x4 trs = input.objects[instance].transform;
               	material = input.objects[instance].material_offset;
                GeometryCluster cluster = input.clusters[input.instances[meshletID].cluster];
                uint i0 = uint(consts.ibuf[cluster.index_offset + (3 * triangleID) + 0]) + cluster.vertex_offset;
                uint i1 = uint(consts.ibuf[cluster.index_offset + (3 * triangleID) + 1]) + cluster.vertex_offset;
                uint i2 = uint(consts.ibuf[cluster.index_offset + (3 * triangleID) + 2]) + cluster.vertex_offset;

                p0 = mul(float4(consts.vertex_pos[i0], 1.0), trs).xyz;
                p1 = mul(float4(consts.vertex_pos[i1], 1.0), trs).xyz;
                p2 = mul(float4(consts.vertex_pos[i2], 1.0), trs).xyz;
                
		uv0 = consts.vertex_uv[i0];
                uv1 = consts.vertex_uv[i1];
                uv2 = consts.vertex_uv[i2];

		vn0 = consts.vertex_nor_tan[i0];
		vn1 = consts.vertex_nor_tan[i1];
		vn2 = consts.vertex_nor_tan[i2];

		trs_invT = float3x3(
			cross(trs[1].xyz, trs[2].xyz),
			cross(trs[2].xyz, trs[0].xyz),
			cross(trs[0].xyz, trs[1].xyz)	
		);
	}

	MaterialPBR mtl = input.materials[material];

	bool is_backface;
	float hit_depth;
	float2 bary = compute_barycentrics(ray_origin, ray_direction, p0, p1, p2, hit_depth, is_backface);
	float3 p = ray_origin + ray_direction * hit_depth;
	float4 tmp = mul(float4(p, 1.0), consts.camera);
	tmp.xyz /= tmp.w;

	float3 n0_raw = oct_to_float3(vn0.x);
	float3 n1_raw = oct_to_float3(vn1.x);
	float3 n2_raw = oct_to_float3(vn2.x);

	float3 n0 = normalize(mul(n0_raw, trs_invT));
	float3 n1 = normalize(mul(n1_raw, trs_invT));
	float3 n2 = normalize(mul(n2_raw, trs_invT));
	float3 normal_raw = attribute_at_bary(n0, n1, n2, bary); 

	float3 N = normalize(normal_raw);	
	
	float2 bary_quad_x = compute_barycentrics(ray_origin, ray_direction_quad_x, p0, p1, p2);
	float2 bary_quad_y = compute_barycentrics(ray_origin, ray_direction_quad_y, p0, p1, p2);

	float2 uvs = attribute_at_bary(uv0, uv1, uv2, bary);
	float2 uv_dx = uvs - attribute_at_bary(uv0, uv1, uv2, bary_quad_x);
	float2 uv_dy = uvs - attribute_at_bary(uv0, uv1, uv2, bary_quad_y);
	float3 V = normalize(consts.cam_pos.xyz - p);
	
	ResourceHandle<SamplerState> smp_aniso = {1u};
	float3 albedo = (mtl.albedo.valid() ? mtl.albedo.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xyz : float3(1.0)) * mtl.diffuse_factor.xyz;
	float3 emissive = (mtl.emissive.valid() ? mtl.emissive.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xyz : float3(1.0)) * mtl.emissive_factor.xyz;
	float3 physical = mtl.mro.valid() ? mtl.mro.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xyz : float3(1.0, 1.0, 0.0);

	if(mtl.normalmap.valid() && dot(V, V) < 1000)
	{
		float2 normalmap = mtl.normalmap.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xy;
		
		float4 tan0_raw = decode_tangent(n0_raw, asfloat(vn0.y));
		float4 tan1_raw = decode_tangent(n1_raw, asfloat(vn1.y));
		float4 tan2_raw = decode_tangent(n2_raw, asfloat(vn2.y));

		float4 tan0 = float4(normalize(mul(tan0_raw.xyz, trs_invT)), tan0_raw.w);
		float4 tan1 = float4(normalize(mul(tan1_raw.xyz, trs_invT)), tan1_raw.w);
		float4 tan2 = float4(normalize(mul(tan2_raw.xyz, trs_invT)), tan2_raw.w);
		float4 tangent = attribute_at_bary(tan0, tan1, tan2, bary);

		float3 bitangent = tangent.w * cross(tangent.xyz, normal_raw);
		float3 nmap_val = decompress_bc5n((normalmap * 2.0 - 1.0) * mtl.normal_factor * 0.33);
		N = normalize(mul(nmap_val, float3x3(tangent.xyz, bitangent, normal_raw)));
	}

	if(is_backface)
		N = -N;

	float roughness = physical.r * mtl.roughness_factor;
	float metallic = physical.g * mtl.metallic_factor;
	
	float NdotV = dot(N, V);
	N += (2.0 * saturate(-NdotV)) * V;

	float3 amb_IRR = consts.irradiance.desc.SampleLevel(smp.desc, N, 0) * consts.ambient_factor;

	NdotV = dot(N, V);

	float3 l0 = float3(0.0);
	
	float reflectance = 0.16 * mtl.reflectivity * mtl.reflectivity;
	float3 diffuse = ((1.0 - metallic) * albedo) / float.getPi();
	float3 f0 = float3(reflectance * (1.0 - metallic)) + albedo * metallic;
	float3 f90 = saturate(50.0 * f0);

	float2 envBRDF = consts.brdf.desc.SampleLevel(smp.desc, float2(max(NdotV, 0.0), roughness), 0);

	float3 reflection = reflect(-V, N);
	float3 amb_PFC = consts.prefiltered.desc.SampleLevel(smp.desc, reflection, roughness * 8.0) * consts.ambient_factor;

	roughness = max(roughness, 0.045);
	l0 += calculate_direct_lighting(N, V, NdotV, diffuse, roughness, f0, p, normalize(normal_raw), tmp.z);

	float horizon = min(1.0 + dot(reflection, N), 1.0);
	float3 amb_SPEC = amb_PFC * (f0 * envBRDF.x + f90 * envBRDF.y) * horizon * horizon;
	float3 ambient = diffuse * amb_IRR + amb_SPEC;
	input.framebuffer.desc[pixel] = (ambient + l0 + emissive) * consts.exposure;

	}
}
