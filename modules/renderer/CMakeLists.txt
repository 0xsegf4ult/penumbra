add_library(penumbra_renderer STATIC "")

target_link_libraries(penumbra_renderer PRIVATE penumbra_core penumbra_math penumbra_gpu penumbra_ui)

target_sources(penumbra_renderer PUBLIC FILE_SET CXX_MODULES
	FILES
	api.cppm
	geometry_buffer.cppm
	mod.cppm
	render_world.cppm
	PRIVATE
	impl.cpp)
