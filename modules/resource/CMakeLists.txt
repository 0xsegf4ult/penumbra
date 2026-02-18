add_library(penumbra_resource STATIC "")

target_link_libraries(penumbra_resource PRIVATE penumbra_core penumbra_math penumbra_gpu penumbra_renderer)

target_sources(penumbra_resource PUBLIC FILE_SET CXX_MODULES
	FILES
	geometry.cppm
	resource_manager.cppm)
