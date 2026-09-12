#pragma once

#include <penumbra/gpu.hpp>
#include <penumbra/types.hpp>

namespace penumbra
{

GPUTextureDescriptor renderer_brdf_init();
void renderer_brdf_cleanup();

}
