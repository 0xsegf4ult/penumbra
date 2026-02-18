#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<uint, Access.Read> visibility;
	Ptr<IndirectCommand, Access.ReadWrite> commands;
	Ptr<GeometryCluster, Access.Read> clusters;
	uint count;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void computeMain(uint DTid: SV_DispatchThreadID, uniform CSInput input)
{
	if(DTid >= input.count)
		return;

	if(input.visibility[DTid] == 0)
		return;

	ClusterInstance instance = input.instances[DTid];
	GeometryCluster cluster = input.clusters[instance.cluster];
	
	IndirectCommand cmd;
	cmd.index_count = cluster.index_count;
	cmd.instance_count = 1u;
	cmd.index_offset = cluster.index_offset;
	cmd.vertex_offset = cluster.vertex_offset;
	cmd.instance_id = DTid;

	input.commands[DTid] = cmd;
}
