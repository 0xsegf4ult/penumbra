add_library(penumbra_ui STATIC "")

target_link_libraries(penumbra_ui PRIVATE penumbra_core penumbra_gpu PUBLIC imgui)
target_include_directories(penumbra_ui PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_sources(penumbra_ui 
	PRIVATE
	device_overlay.cpp
	imgui.cpp)

