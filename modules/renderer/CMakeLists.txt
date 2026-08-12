add_library(penumbra_renderer STATIC "")

target_link_libraries(penumbra_renderer PRIVATE penumbra_core penumbra_gpu penumbra_ui)
target_include_directories(penumbra_renderer PUBLIC ${CMAKE_SOURCE_DIR}/include PRIVATE ${CMAKE_SOURCE_DIR}/modules)
target_sources(penumbra_renderer PRIVATE
	bloom.cpp
	brdf.cpp
	resource.cpp
	renderer.cpp
	shadowmap.cpp
	transparent.cpp
	visbuffer.cpp
	world.cpp)
