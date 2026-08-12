add_library(penumbra_gpu STATIC "")

find_package(SDL3 REQUIRED)

target_link_libraries(penumbra_gpu PRIVATE volk penumbra_core SDL3::SDL3)
target_include_directories(penumbra_gpu PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_sources(penumbra_gpu PRIVATE
	shader.cpp
	vulkan.cpp)     
