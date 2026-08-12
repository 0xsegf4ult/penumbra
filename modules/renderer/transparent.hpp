#pragma once

namespace penumbra
{

struct GPUCommandBuffer;
struct visibility_buffer;

void renderer_transparent_init();
void renderer_transparent_cleanup();
void renderer_transparent_draw(visibility_buffer& visbuffer, GPUCommandBuffer& cmd);

}
