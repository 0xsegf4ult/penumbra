add_library(penumbra_ui STATIC "")

target_link_libraries(penumbra_ui PRIVATE penumbra_core penumbra_math penumbra_gpu PUBLIC imgui-port)
target_sources(penumbra_ui PUBLIC FILE_SET CXX_MODULES
	FILES
	imgui.cppm
	device_overlay.cppm
	mod.cppm)

