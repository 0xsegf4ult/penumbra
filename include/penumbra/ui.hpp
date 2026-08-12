#pragma once

#include <penumbra/types.hpp>
#include <penumbra/window.hpp>
#include <penumbra/math/vector.hpp>

#include <imgui_internal.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ImGuizmo.h>

#include <functional>

namespace penumbra
{

struct GPUCommandBuffer;

bool imgui_backend_init(window_t window);
void imgui_backend_shutdown();
void imgui_backend_render(GPUCommandBuffer& cmd, double dt);
void imgui_add_hook(std::function<void()>&& hook);

namespace ui
{

void draw_device_overlay(uvec2 root = {0u, 0u});

}

}
