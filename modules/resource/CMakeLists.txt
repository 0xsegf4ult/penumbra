add_library(penumbra_resource STATIC "")

target_link_libraries(penumbra_resource PRIVATE penumbra_core penumbra_gpu penumbra_renderer)
target_include_directories(penumbra_resource PUBLIC ${PROJECT_SOURCE_DIR}/include PRIVATE ${PROJECT_SOURCE_DIR}/modules)
target_sources(penumbra_resource
	PRIVATE
	resource.cpp)
