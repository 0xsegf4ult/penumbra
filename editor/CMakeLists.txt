add_executable(editor)

target_include_directories(editor PRIVATE ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(editor PUBLIC penumbra::modules PRIVATE fastgltf::module meshoptimizer ispc_texture_compressor)
target_sources(editor PUBLIC FILE_SET CXX_MODULES
	FILES
	core.cppm
	import/geometry.cppm
	import/gltf.cppm
	import/texture.cppm
	widgets/inspector.cppm
	widgets/scenegraph.cppm
	widgets/viewport.cppm
	widgets/widget.cppm
	world/components/animation.cppm
	world/components/camera.cppm
	world/components/lights.cppm
	world/components/render_object.cppm
	world/envmap.cppm
	world/prefab.cppm
	world/state.cppm
	PRIVATE main.cpp)
set_target_properties(editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
