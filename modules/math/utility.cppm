export module penumbra.math:utility;

import std;

export namespace penumbra
{

template <std::floating_point T>
constexpr T to_radians(T degrees)
{
	return degrees * (std::numbers::pi_v<T> / T(180.0));
}

template <typename T, typename L>
constexpr T mix(const T& a, const T& b, const L& lerp)
{
	return a * (L(1.0) - lerp) + b * lerp;
}

constexpr float fp_epsilon = std::numeric_limits<float>::epsilon();
constexpr float fp_epsilon_sqr = fp_epsilon * fp_epsilon;

}
