#include "normal_encoding.slang"

struct geom_skinned_format
{
	float3 pos;
	float tangentQ;
	float2 uv;
	uint nrmOct;
	uint joints;
	float4 weights;
};

struct geom_nor_tan_format
{
	uint nrmOct;
	float tangentQ;
};

struct CSData
{
	Ptr<geom_skinned_format, Access.Read> skinned_vertices;
	Ptr<float3, Access.ReadWrite> vertex_pos;
	Ptr<float2, Access.ReadWrite> vertex_uv;
	Ptr<geom_nor_tan_format, Access.ReadWrite> vertex_nor_tan;
	Ptr<float4x4, Access.Read> bones;
	uint vertex_count;
};

[shader("compute")]
[numthreads(32, 1, 1)]
void computeMain(uint DTid : SV_DispatchThreadID, uniform CSData data)
{
	if(DTid >= data.vertex_count) 
		return;

	Ptr<geom_skinned_format, Access.Read> sk_vtx = data.skinned_vertices + DTid;
	
	float3 pos = sk_vtx->pos;
	float3 nrm = oct_to_float3(sk_vtx->nrmOct);
	float4 tang = decode_tangent(nrm, sk_vtx->tangentQ);

	if(any(sk_vtx->weights != float4(0.0f)))
	{
		float4 p = float4(0.0f);
		float3 n = float3(0.0f);
		float3 t = float3(0.0f);

		float wsum = 0.0f;

		for(uint i = 0; ((i < 4) && (wsum < 1.0f)); i++)
		{
			uint shift = i * 8;
			uint bone = ((sk_vtx->joints >> shift) & 0xFF);
			
			p += mul(float4(pos, 1.0f), data.bones[bone]) * sk_vtx->weights[i];
			n += mul(float3x3(data.bones[bone]), nrm) * sk_vtx->weights[i];
			t += mul(float3x3(data.bones[bone]), tang.xyz) * sk_vtx->weights[i];
			wsum += sk_vtx->weights[i];
		}
		pos = p.xyz;
		nrm = n.xyz;
		tang.xyz = t.xyz;
	}

	data.vertex_pos[DTid] = pos;
	data.vertex_uv[DTid] = sk_vtx->uv;
	data.vertex_nor_tan[DTid].nrmOct = float3_to_oct(nrm);
	data.vertex_nor_tan[DTid].tangentQ = encode_tangent(nrm, tang.xyz, tang.w < 0.0);
} 
