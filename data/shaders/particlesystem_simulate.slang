#include "particlesystem.slang"

struct CSData
{
	Ptr<Particle, Access.ReadWrite> particles;
	Ptr<uint, Access.Read> alive_list0;
	Ptr<uint, Access.ReadWrite> alive_list1;
	Ptr<uint, Access.ReadWrite> dead_list;
	Ptr<PSCounters, Access.ReadWrite> counters;
	Ptr<PSArguments, Access.ReadWrite> arguments;
	float dt;
};

ConstantBuffer<Emitter> emitter;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint DTid : SV_DispatchThreadID, uniform CSData data)
{
	if(DTid == 0)
		data.counters->dead = max(0, data.counters->dead);

	GroupMemoryBarrierWithGroupSync();

	if(DTid >= data.counters->alive)
		return;

	uint pidx = data.alive_list0[DTid];
	Ptr<Particle, Access.ReadWrite> particle = data.particles + pidx;

	particle->life -= data.dt;

	if(particle->life > 0.0)
	{
		particle->force += emitter.force;
		particle->velocity += particle->force * data.dt;
		particle->position += particle->velocity * data.dt;

		particle->force = float3(0.0);
		particle->velocity *= emitter.drag;
		const float life_lerp = 1.0 - particle->life / particle->max_life;
		particle->size = lerp(particle->size_begin_end.x, particle->size_begin_end.y, life_lerp);
		uint new_idx;
		InterlockedAdd(data.counters->alive_sim, 1u, new_idx);
		data.alive_list1[new_idx] = pidx;

		InterlockedAdd(data.arguments->draw.x, 6u);
	}
	else
	{
		int dead_idx;
		InterlockedAdd(data.counters->dead, 1, dead_idx);
		data.dead_list[dead_idx] = pidx;
	}
}
