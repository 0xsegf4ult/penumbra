add_executable(editor)

target_include_directories(editor PRIVATE ${CMAKE_SOURCE_DIR}/external ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(editor PUBLIC penumbra::modules PRIVATE fastgltf meshoptimizer ispc_texture_compressor)
target_sources(editor PRIVATE
	import/geometry.cpp
	import/texture.cpp
	import/gltf.cpp
	import/qmap.cpp
	world/prefab.cpp
	world/state.cpp
	widgets/console.cpp
	widgets/inspector.cpp
	widgets/scenegraph.cpp
	widgets/viewport.cpp
	core.cpp
	main.cpp)
set_target_properties(editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
