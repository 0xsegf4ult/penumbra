#pragma once

#include <penumbra/types.hpp>
#include <span>

namespace penumbra
{

void audio_init();
void audio_shutdown();
void audio_play(std::span<u8> data, void* spec);

}
