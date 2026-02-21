#pragma once

float2 compute_barycentrics(float3 ray_origin, float3 ray_direction, float3 p0, float3 p1, float3 p2)
{
        float3 v0v1 = p1 - p0;
        float3 v0v2 = p2 - p0;
        float3 pvec = cross(ray_direction, v0v2);
        float det_rcp = rcp(dot(v0v1, pvec));
        float3 tvec = ray_origin - p0;
        float u = dot(tvec, pvec) * det_rcp;
        float3 qvec = cross(tvec, v0v1);
        float v = dot(ray_direction, qvec) * det_rcp;
        return float2(u, v);
}

float2 compute_barycentrics(float3 ray_origin, float3 ray_direction, float3 p0, float3 p1, float3 p2, out float t, out bool is_backface)
{
        float3 v0v1 = p1 - p0;
        float3 v0v2 = p2 - p0;
        float3 pvec = cross(ray_direction, v0v2);
        float det = dot(v0v1, pvec);
        is_backface = det < 0;
        float det_rcp = rcp(det);
        float3 tvec = ray_origin - p0;
        float u = dot(tvec, pvec) * det_rcp;
        float3 qvec = cross(tvec, v0v1);
        float v = dot(ray_direction, qvec) * det_rcp;
        t = dot(v0v2, qvec) * det_rcp;
        return float2(u, v);
}

float2 attribute_at_bary(float2 a0, float2 a1, float2 a2, float2 bary)
{
        return mad(a0, 1 - bary.x - bary.y, mad(a1, bary.x, a2 * bary.y));
}

float3 attribute_at_bary(float3 a0, float3 a1, float3 a2, float2 bary)
{
        return mad(a0, 1 - bary.x - bary.y, mad(a1, bary.x, a2 * bary.y));
}

float4 attribute_at_bary(float4 a0, float4 a1, float4 a2, float2 bary)
{
        return mad(a0, 1 - bary.x - bary.y, mad(a1, bary.x, a2 * bary.y));
}
