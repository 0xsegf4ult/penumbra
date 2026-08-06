#include "particlesystem.slang"

struct CSData
{
	Ptr<PSCounters, Access.ReadWrite> counters; 
	Ptr<PSArguments, Access.ReadWrite> arguments;
	uint emit_target;
};

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uniform CSData data)
{
	data.counters->alive = data.counters->alive_sim;
	data.counters->dead = max(0, data.counters->dead);
	data.counters->emit = min(data.emit_target, data.counters->dead);
	data.counters->alive_sim = 0u;

	data.arguments->emit = uint3((data.counters->emit + 63u) / 64u, 1u, 1u);
	data.arguments->simulate = uint3((data.counters->emit + data.counters->alive + 63u) / 64u, 1u, 1u);
	data.arguments->draw = uint4(0u, 1u, 0u, 0u);
}
