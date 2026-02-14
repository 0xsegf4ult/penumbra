#pragma once

[[allow("parameterBindingsOverlap")]]
[[vk::binding(0, 0)]]
Texture2D[] texture_heap;

[[allow("parameterBindingsOverlap")]]
[[vk::binding(0, 0)]]
TextureCube[] cube_texture_heap;

[[vk::binding(0, 1)]]
RWTexture2D[] rwtexture_heap;

[[vk::binding(0, 2)]]
SamplerState[] sampler_heap;

uint pack_rgba(float4 value)
{
        uint result = 0;
        result |= (uint)(value.x * 255.0) << 0u;
        result |= (uint)(value.y * 255.0) << 8u;
        result |= (uint)(value.z * 255.0) << 16u;
        result |= (uint)(value.w * 255.0) << 24u;
        return result;
}

float4 unpack_rgba(uint value)
{
        float4 result;
        result.r = (float)((value >> 0u) & 0xFF) / 255.0;
        result.g = (float)((value >> 8u) & 0xFF) / 255.0;
        result.b = (float)((value >> 16u) & 0xFF) / 255.0;
        result.a = (float)((value >> 24u) & 0xFF) / 255.0;
        return result;
}

float3 apply_srgb_curve(float3 x)
{
        return select(x < 0.0031308, 12.92 * x, 1.13005 * sqrt(x - 0.00228) - 0.13488 * x + 0.005719);
}

float3 remove_srgb_curve(float3 x)
{
        return select(x < 0.04045, x / 12.92, -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864);
}
