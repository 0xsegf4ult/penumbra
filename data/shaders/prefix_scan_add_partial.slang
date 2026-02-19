struct CSInput
{
	Ptr<uint, Access.Read> inputs;
	Ptr<uint, Access.ReadWrite> outputs;
	uint count;
};

groupshared uint sum;

[shader("compute")]
[numthreads(512, 1, 1)]
void computeMain(uniform CSInput input, uint DTid : SV_DispatchThreadID, uint GTid : SV_GroupThreadID, uint Gid : SV_GroupID)
{
	if(Gid > 0 && DTid < input.count)
	{
		sum = 0;
		if(GTid == 0)
			sum = input.inputs[Gid - 1];

		GroupMemoryBarrierWithGroupSync();

		input.outputs[DTid] += sum;
	}
}
