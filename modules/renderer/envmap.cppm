export module penumbra.renderer:envmap;
import penumbra.gpu;

namespace penumbra
{

export struct RenderEnvironmentMap
{
	GPUTextureDescriptor irradiance;
	GPUTextureDescriptor prefiltered;
};

}

