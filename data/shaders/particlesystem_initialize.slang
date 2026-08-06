#include "particlesystem.slang"

struct CSData
{
	Ptr<PSCounters, Access.ReadWrite> counters;
	Ptr<uint, Access.ReadWrite> dead_list;
	uint count;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void computeMain(uint DTid : SV_DispatchThreadID, uniform CSData data)
{
	if(DTid == 0)
		data.counters->dead = data.count;

	if(DTid >= data.count)
		return;

	data.dead_list[DTid] = DTid;
}
