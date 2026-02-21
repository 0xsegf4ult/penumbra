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
	ResourceHandle<RWTexture2D> framebuffer;
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

float3 calculate_direct_lighting(float3 N, float3 V, float NdotV, float3 diffuse, float roughness, float3 f0)
{
	float3 L = normalize(-consts.light_direction.xyz);
	float NdotL = saturate(dot(N, L));
	float3 H = normalize(V + L);
	float NdotH = dot(N, H);
	
	float NDF = DistributionGGX(max(NdotH, 0.0), roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	float3 F = fresnelSchlick(saturate(dot(H, V)), f0);
	
	float denom = rcp(4.0 * max(NdotV, 0.0) * NdotL + 0.0001);
	float3 specular_contrib = NDF * G * F * denom;

	float3 luminance = consts.light_color.rgb * NdotL;
	float3 l0 = (diffuse + specular_contrib) * luminance;
	return l0;
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

	if(primitiveID == 0)
	{
		input.framebuffer.desc[pixel] = float4(1.0);
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

	MaterialPBR mtl = input.materials[material > 0 ? material - 1 : 0];

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
	float3 cam_pos = consts.inverse_view[3].xyz;
	float3 V = normalize(cam_pos - p);
	
	ResourceHandle<SamplerState> smp_aniso = {1u};
	float3 albedo = (mtl.albedo.valid() ? mtl.albedo.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xyz : float3(1.0)) * mtl.diffuse_factor.xyz;
	float3 emissive = (mtl.emissive.valid() ? mtl.emissive.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xyz : float3(1.0)) * mtl.emissive_factor.xyz;
	float3 physical = mtl.mro.valid() ? mtl.mro.desc.SampleGrad(smp_aniso.desc, uvs, uv_dx, uv_dy).xyz : float3(1.0, 1.0, 0.0);
	
	if(is_backface)
		N = -N;

	float roughness = physical.r * mtl.roughness_factor;
	float metallic = physical.g * mtl.metallic_factor;
	
	float NdotV = dot(N, V);
	N += (2.0 * saturate(-NdotV)) * V;
	NdotV = dot(N, V);

	float3 l0 = float3(0.0);
	
	float reflectance = 0.16 * mtl.reflectivity * mtl.reflectivity;
	float3 diffuse = ((1.0 - metallic) * albedo) / float.getPi();
	float3 f0 = float3(reflectance * (1.0 - metallic)) + albedo * metallic;
	float3 f90 = saturate(50.0 * f0);

	roughness = max(roughness, 0.045);
	l0 += calculate_direct_lighting(N, V, NdotV, diffuse, roughness, f0);

	float3 ambient = float3(0.005);
	float3 hdr = float3(ambient + l0 + emissive) * consts.exposure;
	input.framebuffer.desc[pixel] = float4(applyACES(hdr), 1.0); 

	}
}
