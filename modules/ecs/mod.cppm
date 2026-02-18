module;

#define NOMINMAX
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

export module penumbra.ecs;

export namespace penumbra::ecs
{
	using entt::registry;
	using entt::entity;
	using entt::null;
}
