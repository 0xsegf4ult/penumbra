#include "globals.slang"

struct CSInput
{
	Ptr<uint, Access.Read> inputs;
	Ptr<uint, Access.ReadWrite> outputs;
	Ptr<uint, Access.ReadWrite> partial_sums;
	uint count;
};

groupshared uint sdata[32];

[shader("compute")]
[numthreads(512, 1, 1)]
void computeMain(uniform CSInput input, uint DTid : SV_DispatchThreadID, uint GTid : SV_GroupThreadID, uint Gid : SV_GroupID)
{
	uint sum = 0;
	if(DTid < input.count)
		sum = input.inputs[DTid];

	sum = WavePrefixInclusiveAdd(sum);

	if(WaveGetLaneIndex() == WaveGetLaneCount() - 1)
		sdata[SubgroupID()] = sum;

	GroupMemoryBarrierWithGroupSync();

	if(SubgroupID() == 0)
	{
		uint warpSum = WaveGetLaneIndex() < WaveGetLaneCount() ? sdata[WaveGetLaneIndex()] : 0;
		warpSum = WavePrefixInclusiveAdd(warpSum);
		sdata[WaveGetLaneIndex()] = warpSum;
	}

	GroupMemoryBarrierWithGroupSync();

	uint blockSum = 0;
	if(SubgroupID() > 0)
		blockSum = sdata[SubgroupID() - 1];

	sum += blockSum;

	if(DTid < input.count)
		input.outputs[DTid] = sum;

	if(GTid == 511)
		input.partial_sums[Gid] = sum;
}

