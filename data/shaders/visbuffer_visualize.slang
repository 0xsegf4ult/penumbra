#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<ClusterInstance, Access.Read> instances;
	uint2 res;
	ResourceHandle<Texture2D<uint>> visbuffer;
	ResourceHandle<RWTexture2D> framebuffer;
};

uint hash(uint a)
{
   a = (a+0x7ed55d16) + (a<<12);
   a = (a^0xc761c23c) ^ (a>>19);
   a = (a+0x165667b1) + (a<<5);
   a = (a+0xd3a2646c) ^ (a<<9);
   a = (a+0xfd7046c5) + (a<<3);
   a = (a^0xb55a4f09) ^ (a>>16);
   return a;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint groupIndex : SV_GroupIndex, uint2 Gid : SV_GroupID, uniform CSInput input)
{
	const uint2 invocation = remap_lane_8x8(groupIndex);
	const uint2 pixel = Gid.xy * 8 + invocation.xy;

	if(pixel.x >= input.res.x || pixel.y >= input.res.y)
		return;

	uint primitiveID = input.visbuffer.desc[pixel];
	if(primitiveID == 0)
	{
		input.framebuffer.desc[pixel] = float4(1.0);
		return;
	}

	uint meshletID = (primitiveID >> 7) - 1;
	uint cluster = hash(input.instances[meshletID].cluster);

	uint b = (cluster >> 16) & 0xFF;
	uint g = (cluster >> 8) & 0xFF;
	uint r = cluster & 0xFF;

	input.framebuffer.desc[pixel] = float4(r / 255.0, g / 255.0, b / 255.0, 1.0);
}
