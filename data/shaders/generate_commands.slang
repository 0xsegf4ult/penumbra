#include "globals.slang"
#include "render_world.slang"

struct CSInput
{
	Ptr<ClusterInstance, Access.Read> instances;
	Ptr<ObjectData, Access.Read> objects;
	Ptr<uint, Access.Read> visibility;
	Ptr<uint, Access.Read> visibility_prefixsum;
	Ptr<RenderBucket, Access.Read> buckets;
	Ptr<IndirectCommand, Access.ReadWrite> commands;
	Ptr<GeometryCluster, Access.Read> clusters;
	uint count;
	int is_visbuffer;
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
	uint bucket = input.objects[instance.object - 1].pack_bucket_lcount >> 16;
	uint bOffset = 0;
	if(bucket > 0)
	{
		uint dsum = 0;
		for(uint i = 0; i < bucket; i++)
			dsum += input.buckets[i].count;
		
		bOffset = input.buckets[bucket].offset - dsum;
	}

	GeometryCluster cluster = input.clusters[instance.cluster];
	
	IndirectCommand cmd;
	cmd.index_count = cluster.index_count;
	cmd.instance_count = 1u;
	cmd.index_offset = cluster.index_offset;
	cmd.vertex_offset = cluster.vertex_offset;
	if(input.is_visbuffer == 1)
	{
		cmd.instance_id = DTid;
	}
	else
	{
		cmd.instance_id = instance.object - 1;
	}

	input.commands[input.visibility_prefixsum[DTid] + bOffset - 1] = cmd;
}
