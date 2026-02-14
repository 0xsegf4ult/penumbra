add_library(penumbra_gpu STATIC "")

find_package(SDL3 REQUIRED)

target_link_libraries(penumbra_gpu PRIVATE volk penumbra_core penumbra_math SDL3::SDL3)

target_sources(penumbra_gpu PUBLIC FILE_SET CXX_MODULES
	FILES
	api.cppm
	shader.cppm
	PRIVATE
	impl_vulkan.cpp)     
