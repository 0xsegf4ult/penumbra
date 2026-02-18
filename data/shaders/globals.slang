#pragma once

export T getDescriptorFromHandle<T>(DescriptorHandle<T> handle) where T : IOpaqueDescriptor
{
        switch(T.kind)
        {
        case DescriptorKind.Sampler:
                return __getDynamicResourceHeap<T>(0)[((uint2)handle).x];
        case DescriptorKind.Texture:
        {
                if(T.descriptorAccess == DescriptorAccess.Read)
                        return __getDynamicResourceHeap<T>(1)[((uint2)handle).x];
                else
                        return __getDynamicResourceHeap<T>(2)[((uint2)handle).x];
        }
        default:
                // should never happen
                return __getDynamicResourceHeap<T>(3)[0];
        }
}

struct ResourceHandle<T> where T: IOpaqueDescriptor
{
        private uint inner;

	__init()
	{
		inner = 0;
	}

	__init(uint value)
	{
		inner = value;
	}

        property DescriptorHandle<T> desc
        {
                get {
                        return (DescriptorHandle<T>)uint2(inner, 0);
                }

        }
};

uint SubgroupID()
{
	return spirv_asm
	{
		result: $$uint = OpLoad builtin(SubgroupId:uint);
	};
}

uint WavePrefixInclusiveAdd(uint sum)
{
	return spirv_asm
	{
		OpCapability GroupNonUniformArithmetic;
		OpGroupNonUniformIAdd $$uint result Subgroup 1 $sum;
	};
}

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

uint2 remap_lane_8x8(uint lane)
{
        return uint2 (
                bitfieldInsert(bitfieldExtract(lane, 2, 3), lane, 0, 1),
                bitfieldInsert(bitfieldExtract(lane, 3, 3), bitfieldExtract(lane, 1, 2), 0, 2)
        );
}

