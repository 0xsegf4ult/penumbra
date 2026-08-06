#include "globals.slang"
#include "particlesystem.slang"

struct CSData
{
	Ptr<Particle, Access.ReadWrite> particles;
	Ptr<uint, Access.ReadWrite> alive_list;
	Ptr<uint, Access.Read> dead_list;
	Ptr<PSCounters, Access.ReadWrite> counters;
	uint frame_counter;
};

ConstantBuffer<Emitter> emitter;

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint DTid : SV_DispatchThreadID, uniform CSData data)
{
	if(DTid >= data.counters->emit)
		return;
	
	int dead_count;
	InterlockedAdd(data.counters->dead, -1, dead_count);
	if(dead_count < 1)
		return;

	RNGState rng = RNGState(uint2(emitter.random_seed, DTid), data.frame_counter);
	
	float3 emit_pos = float3(0.0);
	if((emitter.flags & EmitterFlags.EmitVolume) == 1)
		emit_pos = float3(rng.next_float() * 2.0 - 1.0, rng.next_float() * 2.0 - 1.0, rng.next_float() * 2.0 - 1.0);

	emit_pos = mul(float4(emit_pos, 1.0), emitter.transform).xyz;

	uint pidx = data.dead_list[dead_count - 1];

	Ptr<Particle, Access.ReadWrite> particle = data.particles + pidx;
	particle->position = emit_pos;
	particle->force = float3(0.0);
	particle->velocity = emitter.velocity + (float3(rng.next_float(), rng.next_float(), rng.next_float()) - 0.5) * emitter.random_factor;
	particle->max_life = particle->life + (particle->life * (rng.next_float() - 0.5) * emitter.life_random_factor);
	particle->life = particle->max_life;
	float startSize = emitter.size + emitter.size * (rng.next_float() - 0.5) * emitter.random_factor;
	particle->size_begin_end = float2(startSize, startSize * emitter.size_scaling);

	uint alive_count;
	InterlockedAdd(data.counters->alive, 1, alive_count);
	data.alive_list[alive_count] = pidx;
}	
