export module penumbra.renderer:api;
export import :geometry_buffer;
import penumbra.core;
import penumbra.math;
import penumbra.gpu;

import std;
using std::uint64_t, std::size_t;

namespace penumbra
{

export void renderer_init(Window& wnd);
export void renderer_shutdown();

export void renderer_next_frame();
export void renderer_process_frame(double dt);
export uint32_t renderer_gfx_frame_index();
export uint64_t renderer_resource_transfer_syncval();

}
