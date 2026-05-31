add_library(penumbra_resource STATIC "")

target_link_libraries(penumbra_resource PRIVATE penumbra_core penumbra_math penumbra_gpu penumbra_renderer penumbra_anim)

target_sources(penumbra_resource PUBLIC FILE_SET CXX_MODULES
	FILES
	api.cppm
	animation.cppm
	geometry.cppm
	material.cppm
	mod.cppm
	resource_id.cppm
	skeleton.cppm
	texture.cppm
	PRIVATE
	impl.cpp)
