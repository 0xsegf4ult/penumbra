add_library(penumbra_physics STATIC "")

target_link_libraries(penumbra_physics PRIVATE penumbra_core)
target_include_directories(penumbra_physics PUBLIC ${CMAKE_SOURCE_DIR}/include PRIVATE ${CMAKE_SOURCE_DIR}/modules)
target_sources(penumbra_physics PRIVATE
	body.cpp
	hull.cpp
	shape.cpp
	world.cpp)
