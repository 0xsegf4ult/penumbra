#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<uint, Access.ReadWrite> output;
	uint2 coords;
	ResourceHandle<Texture2D<uint>> visbuffer;
};

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uniform CSInput data)
{
	uint primitiveID = data.visbuffer.desc[data.coords];
	if(primitiveID == 0)
		*data.output = 0u;

	uint meshletID = (primitiveID >> 7) - 1;
	
	if(meshletID < 65536)	
		*(data.output) = data.instances[meshletID].object;
}	
