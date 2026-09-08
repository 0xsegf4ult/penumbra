#pragma once

#include <penumbra/types.hpp>

namespace penumbra
{

void* heap_allocate(size_t size);
void heap_free(void* ptr);

}
