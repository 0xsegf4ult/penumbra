#pragma once

[Flags]
enum EmitterFlags
{
	EmitVolume,
	EmitFromMesh,
	DepthCollision
};

struct Emitter
{
	float4x4 transform;
	float3 force;
	float drag;
	float3 velocity;
	float life;
	float size;
	float size_scaling;
	uint random_seed;
	float random_factor;
	float life_random_factor;
	EmitterFlags flags;
};

struct Particle
{
	float3 position;
	float3 velocity;
	float3 force;
	float2 size_begin_end;
	float size;
	float life;
	float max_life;
};

struct PSCounters
{
	uint alive;
	int dead;
	uint emit;
	uint alive_sim;
};

struct PSArguments
{
	uint3 emit;
	uint3 simulate;
	uint4 draw;
};	
