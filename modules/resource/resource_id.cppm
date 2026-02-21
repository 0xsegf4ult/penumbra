export module penumbra.resource:resource_id;

import std;
using std::uint8_t, std::uint32_t;

namespace penumbra
{

export enum class ResourceType : uint8_t
{
	Invalid,
	Geometry,
	Texture,
	Material,
};

export struct ResourceID
{
public:
	constexpr ResourceID() : internal{0u} {}
	constexpr ResourceID(ResourceType type, uint32_t handle) : internal{std::to_underlying(type) << 24 | handle & 0xFFFFFF} {}

	constexpr ResourceType get_type() const
	{
		return ResourceType{static_cast<uint8_t>(internal >> 24)};
	}

	constexpr uint32_t get_handle() const
	{
		return internal & 0xFFFFFF;
	}
private:
	uint32_t internal;
};

}
