add_executable(editor)

target_link_libraries(editor penumbra::modules)
target_sources(editor PUBLIC FILE_SET CXX_MODULES
	FILES
	core.cppm
	widgets/inspector.cppm
	widgets/scenegraph.cppm
	widgets/widget.cppm
	world/components/camera.cppm
	world/components/lights.cppm
	world/prefab.cppm
	world/state.cppm
	PRIVATE main.cpp)
set_target_properties(editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
